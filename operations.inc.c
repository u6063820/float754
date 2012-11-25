/* Floating point addition and subtraction
 * Licenced under the ISC license (similar to the MIT/Expat license)
 *
 * Copyright (c) 2012 Jörg Mische <bobbl@gmx.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Include file that just works when included into a proper C source file.
   Set the macro width to the bit width of the types. Then the functions
   floatWIDTH_add() and floatWIDTH_sub() are generated. */

   
#define _FUNC_ADD(W) float ## W ## _add
#define FUNC_ADD(W) _FUNC_ADD(W)
#define _FUNC_SUB(W) float ## W ## _sub
#define FUNC_SUB(W) _FUNC_SUB(W)


FLOAT(WIDTH) FUNC_ADD(WIDTH) (FLOAT(WIDTH) af, FLOAT(WIDTH) bf)
{
    UF(WIDTH) a, b, r;
    a.f = af;
    b.f = bf;
    EXP_TYPE(WIDTH) lo_exp = EXTRACT_EXP(WIDTH, a.u);
    EXP_TYPE(WIDTH) hi_exp = EXTRACT_EXP(WIDTH, b.u);
    EXP_TYPE(WIDTH) diff_exp = lo_exp - hi_exp;
    int_fast16_t sum;


    if (diff_exp == 0) {
	if (hi_exp == EXP_MAX(WIDTH)) {
	    // infinity or NaN
	    if ( ((a.u|b.u) & MANT_MASK(WIDTH)) == 0) {
		// two times infinity (mantissa==0)
		if (a.u != b.u) { // already equal except the sign
		    // infinity minus infinity raises an error
		    raise_exception(EXC_INVALID_OP);
		    r.u = NOTANUM(WIDTH);
		} else {
		    r.u = a.u;
		}
	    } else {
		if ( ((a.u&b.u) & SIGNALLING_MASK(WIDTH)) == 0)
		    // one of the two NaNs is signalling 
		    // (highest bit of the mantissa==0)
		    raise_exception(EXC_INVALID_OP);
		r.u = NOTANUM(WIDTH);
	    }

	} else {
	    // same exponent
	    UINT_FAST(WIDTH) a_mant = EXTRACT_MANT(WIDTH, a.u);
	    UINT_FAST(WIDTH) b_mant = EXTRACT_MANT(WIDTH, b.u);
	    r.u = a.u & SIGN_MASK(WIDTH);

	    if (((a.u ^ b.u) & SIGN_MASK(WIDTH)) == 0)
	    {
		// both numbers have the same sign -> addition
		sum = a_mant + b_mant;

		if (hi_exp == EXP_MAX(WIDTH)-1) {
		    // by the addition the exponent is increased by 1
		    // and then it is too big 
		    r.u |= EXP_MASK(WIDTH);
		} else if (hi_exp == 0) {
		    // both numbers are subnormal. If there is an overflow, i.e. the
		    // sum is normal, bit 52 of the result is set, which means that
		    // the exponent is 1 and not 0. Hence the conversion from subnormal
		    // to normal works automatically, only the sign has to be added.
		    r.u |= sum;
		} else {
		    // because of the two implicit ones the result has one digit
		    // to much, i.e. the mantissa has to be shifted right by 1
		    // and the exponent must be increased by 1
		    r.u |= ((UINT_FAST(WIDTH))(hi_exp+1)<<MANT_WIDTH(WIDTH)) 
			+ ((sum + ((sum>>1)&1)) >> 1);
		}
	    } else {
		// numbers have different signs -> subtraction
		sum = a_mant - b_mant;

		if (sum == 0) {
		    r.u = 0;
		} else {
		    if (sum < 0) {
	    		r.u ^= SIGN_MASK(WIDTH);
			sum = -sum;
		    }

		    // very easy for subnormal numbers
		    if (hi_exp == 0) {
			r.u |= sum;

		    } else {
			// normalise mantissa
			uint_fast8_t zeros = CLZ(WIDTH)(sum) - (WIDTH-1-MANT_WIDTH(WIDTH));
			sum <<= zeros;
    			hi_exp -= zeros;
			r.u |= (hi_exp<1)
			    ? (sum >> (1-hi_exp)) // subnormal result
			    : (((UINT_FAST(WIDTH))(hi_exp-1)<<MANT_WIDTH(WIDTH)) + sum);
				// Now the leading 1 must be masked out. But it is more efficient to
				// decrement the exponent by 1 and then add the implicit 1.
		    }
		}
	    }
	}
    } else {
	UINT_FAST(WIDTH) lo_mant, hi_mant;
	if (diff_exp > 0) {
	    hi_exp += diff_exp;
	    lo_exp -= diff_exp;
		// exchange hi_exp and lo_exp
	    lo_mant = EXTRACT_MANT(WIDTH, b.u);
	    hi_mant = EXTRACT_MANT(WIDTH, a.u);
	    r.u = a.u;
	} else {
	    diff_exp = -diff_exp;
	    lo_mant = EXTRACT_MANT(WIDTH, a.u);
	    hi_mant = EXTRACT_MANT(WIDTH, b.u);
	    r.u = b.u;
	}
	// temporarily the lower bits of r.u are not masked out,
	// in order to return the correct value in the case of an exception

	if (hi_exp == EXP_MAX(WIDTH)) {
	    if (hi_mant!=0) {
		if ((hi_mant & SIGNALLING_MASK(WIDTH))==0)
	    	    raise_exception(EXC_INVALID_OP);        // signalling NaN
		r.u = NOTANUM(WIDTH);			// NaN
	    } // else infinity

	} else if (diff_exp > MANT_WIDTH(WIDTH)+2) {		// lo much too low
	    if (lo_exp!=0 || lo_mant!=0)
		raise_exception(EXC_INEXACT);
	} else {

	    // from now on, only the sign of r.u is needed
	    r.u &= SIGN_MASK(WIDTH);

	    if (lo_exp == 0)
    		diff_exp--; // lo subnormal
	    else
    		lo_mant |= 1L<<MANT_WIDTH(WIDTH); // insert implicit 1


#define REM_HALF (2L<<MANT_WIDTH(WIDTH))
	    UINT_FAST(WIDTH) rem = (lo_mant << ((MANT_WIDTH(WIDTH)+2)-diff_exp)) 
		& ((MANT_MASK(WIDTH)<<2)|3);

	    lo_mant >>= diff_exp;
	    hi_mant |= 1L<<MANT_WIDTH(WIDTH);

	    if (((a.u ^ b.u) & SIGN_MASK(WIDTH)) == 0) {
		// same sign
		// simplifies the normalisation, but overflow checks are needed
		sum = hi_mant + lo_mant;

		if (sum < (2L<<MANT_WIDTH(WIDTH))) {		// no bit overflow
		    if (rem == REM_HALF)
			sum = (sum+1) & (~1);    		// round to even
		    else if (rem > REM_HALF)
			sum++;				// round up
		    // else round down (nothing to do)

		    r.u |= (((UINT_FAST(WIDTH))(hi_exp-1)<<MANT_WIDTH(WIDTH)) + sum);
			// Now the leading 1 must be masked out. But it is more efficient to
			// decrement the exponent by 1 and then add the implicit 1.
		} else {					// 1 bit overflow
		    r.u |=  (hi_exp>EXP_MAX(WIDTH))
			? EXP_MASK(WIDTH)	// infinity
			: (((UINT_FAST(WIDTH))hi_exp<<MANT_WIDTH(WIDTH)) + ROUND(sum, rem));
			    // Now the leading 1 must be masked out. But it is more efficient
			    // to decrement the exponent by 1 and then add the implicit 1.
		}
	    } else {
		// not the same sign
		sum = hi_mant - lo_mant - ((rem!=0) ? 1 : 0);

		uint_fast8_t zeros = CLZ(WIDTH)(sum) - (WIDTH-1-MANT_WIDTH(WIDTH));

		sum = (sum << zeros) | (((-rem) & ((MANT_MASK(WIDTH)<<2)|3))
		    >> ((MANT_WIDTH(WIDTH)+2)-zeros));
		rem = (rem << zeros) & ((MANT_MASK(WIDTH)<<2)|3);
		if (rem == REM_HALF)
    		    sum = (sum+1) & (~1);    		// round to even
		else if (rem != 0 && rem < REM_HALF)
		    sum++;

		hi_exp = hi_exp - zeros;
		r.u |= (hi_exp < 1)
		    ? (sum >> (1-hi_exp))
		    : (((UINT_FAST(WIDTH))(hi_exp-1)<<MANT_WIDTH(WIDTH)) + sum);
			// Now the leading 1 must be masked out. But it is more efficient to
			// decrement the exponent by 1 and then add the implicit 1.
	    }
	}
    }
    return r.f;
}


FLOAT(WIDTH) FUNC_SUB(WIDTH) (FLOAT(WIDTH) af, FLOAT(WIDTH) bf)
{
    UF(WIDTH) b;
    b.f = bf;
    b.u ^= SIGN_MASK(WIDTH);
    return FUNC_SUB(WIDTH) (af, b.f);
}


#undef _FUNC_ADD
#undef FUNC_ADD
#undef _FUNC_SUB
#undef FUNC_SUB

#undef REM_HALF
