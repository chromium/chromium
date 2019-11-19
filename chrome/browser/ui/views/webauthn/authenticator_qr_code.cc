// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_qr_code.h"

#include <string.h>

#include "base/logging.h"

static_assert(AuthenticatorQRCode::kNumSegments != 0 &&
                  AuthenticatorQRCode::kTotalBytes %
                          AuthenticatorQRCode::kNumSegments ==
                      0,
              "invalid configuration");

// Generate generates a QR code containing the given data and returns a
// pointer to an array of kSize×kSize bytes where the least-significant bit of
// each byte is set if that tile should be "black".
base::span<const uint8_t, AuthenticatorQRCode::kTotalSize>
AuthenticatorQRCode::Generate(const uint8_t in[kInputBytes]) {
  memset(d_, 0, sizeof(d_));
  PutVerticalTiming(6);
  PutHorizontalTiming(6);
  PutFinder(3, 3);
  PutFinder(3, kSize - 4);
  PutFinder(kSize - 4, 3);
  // See table E.1 for the location of alignment symbols.
  PutAlignment(22, 22);

  // kFormatInformation is the encoded formatting word for the QR code that
  // this code generates:
  //                  11 011
  //                  --|---
  // error correction Q | Mask pattern 3
  //
  // It's translated into the following, 15-bit value using the table on page
  // 80.
  constexpr uint16_t kFormatInformation = 0x3a06;
  PutFormatBits(kFormatInformation);

  // QR codes require some framing of the data which requires 12 bits of
  // overhead. Since 12 is not a multiple of eight, a frame-shift of all
  // subsequent bytes is required.
  uint8_t prefixed_data[kDataBytes];
  prefixed_data[0] = 0x42;
  prefixed_data[1] = 0x00 | (in[0] >> 4);
  for (size_t i = 0; i < kInputBytes - 1; i++) {
    prefixed_data[i + 2] = (in[i] << 4) | (in[i + 1] >> 4);
  }
  prefixed_data[kDataBytes - 1] = in[kInputBytes - 1] << 4;

  // Each segment of input data is expanded with error correcting
  // information and then interleaved.
  uint8_t expanded_segments[kNumSegments][kSegmentBytes];
  for (size_t i = 0; i < kNumSegments; i++) {
    AddErrorCorrection(&expanded_segments[i][0],
                       &prefixed_data[kSegmentDataBytes * i]);
  }

  uint8_t interleaved_data[kTotalBytes];
  static_assert(kTotalBytes == kSegmentBytes * kNumSegments, "internal error");
  size_t k = 0;
  for (size_t j = 0; j < kSegmentBytes; j++) {
    for (size_t i = 0; i < kNumSegments; i++) {
      interleaved_data[k++] = expanded_segments[i][j];
    }
  }

  // The mask pattern is fixed for this implementation. A full implementation
  // would generate QR codes with every mask pattern and evaluate a quality
  // score, ultimately picking the optimal pattern. Here it's assumed that a
  // different QR code will soon be generated so any random issues will be
  // transient.
  PutBits(interleaved_data, sizeof(interleaved_data), MaskFunction3);

  return d_;
}

// MaskFunction3 implements one of the data-masking functions. See figure 21.
// static
uint8_t AuthenticatorQRCode::MaskFunction3(int x, int y) {
  return (x + y) % 3 == 0;
}

// PutFinder paints a finder symbol at the given coordinates.
void AuthenticatorQRCode::PutFinder(int x, int y) {
  DCHECK_GE(x, 3);
  DCHECK_GE(y, 3);
  fillAt(x - 3, y - 3, 7, 0b11);
  fillAt(x - 2, y - 2, 5, 0b10);
  fillAt(x - 2, y + 2, 5, 0b10);
  fillAt(x - 3, y + 3, 7, 0b11);

  static constexpr uint8_t kLine[7] = {0b11, 0b10, 0b11, 0b11,
                                       0b11, 0b10, 0b11};
  copyTo(x - 3, y - 1, kLine, sizeof(kLine));
  copyTo(x - 3, y, kLine, sizeof(kLine));
  copyTo(x - 3, y + 1, kLine, sizeof(kLine));

  at(x - 3, y - 2) = 0b11;
  at(x + 3, y - 2) = 0b11;
  at(x - 3, y + 2) = 0b11;
  at(x + 3, y + 2) = 0b11;

  for (int xx = x - 4; xx <= x + 4; xx++) {
    clipped(xx, y - 4) = 0b10;
    clipped(xx, y + 4) = 0b10;
  }
  for (int yy = y - 3; yy <= y + 3; yy++) {
    clipped(x - 4, yy) = 0b10;
    clipped(x + 4, yy) = 0b10;
  }
}

// PutAlignment paints an alignment symbol centered at the given coordinates.
void AuthenticatorQRCode::PutAlignment(int x, int y) {
  fillAt(x - 2, y - 2, 5, 0b11);
  fillAt(x - 2, y + 2, 5, 0b11);
  static constexpr uint8_t kLine[5] = {0b11, 0b10, 0b10, 0b10, 0b11};
  copyTo(x - 2, y - 1, kLine, sizeof(kLine));
  copyTo(x - 2, y, kLine, sizeof(kLine));
  copyTo(x - 2, y + 1, kLine, sizeof(kLine));
  at(x, y) = 0b11;
}

// PutVerticalTiming paints the vertical timing signal.
void AuthenticatorQRCode::PutVerticalTiming(int x) {
  for (int y = 0; y < kSize; y++) {
    at(x, y) = 0b10 | (1 ^ (y & 1));
  }
}

// PutVerticalTiming paints the horizontal timing signal.
void AuthenticatorQRCode::PutHorizontalTiming(int y) {
  for (int x = 0; x < kSize; x++) {
    at(x, y) = 0b10 | (1 ^ (x & 1));
  }
}

// PutFormatBits paints the 15-bit, pre-encoded format metadata. See page 56
// for the location of the format bits.
void AuthenticatorQRCode::PutFormatBits(const uint16_t format) {
  // kRun1 is the location of the initial format bits, where the upper nibble
  // is the x coordinate and the lower the y.
  static constexpr uint8_t kRun1[15] = {
      0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x87, 0x88,
      0x78, 0x58, 0x48, 0x38, 0x28, 0x18, 0x08,
  };

  uint16_t v = format;
  for (size_t i = 0; i < sizeof(kRun1); i++) {
    const uint8_t location = kRun1[i];
    at(location >> 4, location & 15) = 0b10 | (v & 1);
    v >>= 1;
  }

  v = format;
  for (int x = kSize - 1; x >= kSize - 1 - 7; x--) {
    at(x, 8) = 0b10 | (v & 1);
    v >>= 1;
  }

  at(8, kSize - 1 - 7) = 0b11;
  for (int y = kSize - 1 - 6; y <= kSize - 1; y++) {
    at(8, y) = 0b10 | (v & 1);
    v >>= 1;
  }
}

// PutBits writes the given data into the QR code in correct order, avoiding
// structural elements that must have already been painted. See section 7.7.3
// about the placement algorithm.
void AuthenticatorQRCode::PutBits(const uint8_t* data,
                                  size_t data_len,
                                  uint8_t (*mask_func)(int, int)) {
  // BitStream vends bits from |data| on demand, in the order that QR codes
  // expect them.
  class BitStream {
   public:
    BitStream(const uint8_t* data, size_t data_len)
        : data_(data), data_len_(data_len), i_(0), bits_in_current_byte_(0) {}

    uint8_t Next() {
      if (bits_in_current_byte_ == 0) {
        if (i_ >= data_len_) {
          byte_ = 0;
        } else {
          byte_ = data_[i_++];
        }
        bits_in_current_byte_ = 8;
      }

      const uint8_t ret = byte_ >> 7;
      byte_ <<= 1;
      bits_in_current_byte_--;
      return ret;
    }

   private:
    const uint8_t* const data_;
    const size_t data_len_;
    size_t i_;
    unsigned bits_in_current_byte_;
    uint8_t byte_;
  };

  BitStream stream(data, data_len);

  bool going_up = true;
  int x = kSize - 1;
  int y = kSize - 1;

  for (;;) {
    uint8_t& right = at(x, y);
    // Test the current value in the QR code to avoid painting over any
    // existing structural elements.
    if (right == 0) {
      right = stream.Next() ^ mask_func(x, y);
    }

    uint8_t& left = at(x - 1, y);
    if (left == 0) {
      left = stream.Next() ^ mask_func(x - 1, y);
    }

    if ((going_up && y == 0) || (!going_up && y == kSize - 1)) {
      if (x == 1) {
        break;
      }
      x -= 2;
      // The vertical timing column is skipped over.
      if (x == 6) {
        x--;
      }
      going_up = !going_up;
    } else {
      if (going_up) {
        y--;
      } else {
        y++;
      }
    }
  }
}

// at returns a reference to the given element of |d_|.
uint8_t& AuthenticatorQRCode::at(int x, int y) {
  DCHECK_LE(0, x);
  DCHECK_LT(x, kSize);
  DCHECK_LE(0, y);
  DCHECK_LT(y, kSize);
  return d_[kSize * y + x];
}

// fillAt sets the |len| elements at (x, y) to |value|.
void AuthenticatorQRCode::fillAt(int x, int y, size_t len, uint8_t value) {
  DCHECK_LE(0, x);
  DCHECK_LE(static_cast<int>(x + len), kSize);
  DCHECK_LE(0, y);
  DCHECK_LT(y, kSize);
  memset(&d_[kSize * y + x], value, len);
}

// copyTo copies |len| elements from |data| to the elements at (x, y).
void AuthenticatorQRCode::copyTo(int x,
                                 int y,
                                 const uint8_t* data,
                                 size_t len) {
  DCHECK_LE(0, x);
  DCHECK_LE(static_cast<int>(x + len), kSize);
  DCHECK_LE(0, y);
  DCHECK_LT(y, kSize);
  memcpy(&d_[kSize * y + x], data, len);
}

// clipped returns a reference to the given element of |d_|, or to
// |clip_dump_| if the coordinates are out of bounds.
uint8_t& AuthenticatorQRCode::clipped(int x, int y) {
  if (0 <= x && x < kSize && 0 <= y && y < kSize) {
    return d_[kSize * y + x];
  }
  return clip_dump_;
}

// GF28Mul returns the product of |a| and |b| (which must be field elements,
// i.e. < 256) in the field GF(2^8) mod x^8 + x^4 + x^3 + x^2 + 1.
// static
uint8_t AuthenticatorQRCode::GF28Mul(uint16_t a, uint16_t b) {
  uint16_t acc = 0;

  // Perform 8-bit, carry-less multiplication of |a| and |b|.
  for (int i = 0; i < 8; i++) {
    const uint16_t mask = ~((b & 1) - 1);
    acc ^= a & mask;
    b >>= 1;
    a <<= 1;
  }

  // Add multiples of the modulus to eliminate all bits past a byte. Note that
  // the bits in |modulus| have a one where there's a non-zero power of |x| in
  // the field modulus.
  uint16_t modulus = 0b100011101 << 7;
  for (int i = 15; i >= 8; i--) {
    const uint16_t mask = ~((acc >> i) - 1);
    acc ^= modulus & mask;
    modulus >>= 1;
  }

  return acc;
}

// AddErrorCorrection writes the Reed-Solomon expanded version of |in| to
// |out|.
// static
void AuthenticatorQRCode::AddErrorCorrection(
    uint8_t out[kSegmentBytes],
    const uint8_t in[kSegmentDataBytes]) {
  // kGenerator is the product of (z - x^i) for 0 <= i < |kSegmentECBytes|,
  // where x is the term of GF(2^8) and z is the term of a polynomial ring
  // over GF(2^8). It's generated with the following Sage script:
  //
  // F.<x> = GF(2^8, modulus = x^8 + x^4 + x^3 + x^2 + 1)
  // R.<z> = PolynomialRing(F, 'z')
  //
  // def toByte(p):
  //     return sum([(1<<i) * int(term) for (i, term) in
  //     enumerate(p.polynomial())])
  //
  // def generatorPoly(n):
  //    acc = (z - F(1))
  //    for i in range(1,n):
  //        acc *= (z - x^i)
  //    return acc
  //
  // gen = generatorPoly(18)
  // coeffs = list(gen)
  // gen = [toByte(x) for x in coeffs]
  // print 'uint8_t kGenerator[' + str(len(gen)) + '] = {' + str(gen) + '}'
  static constexpr uint8_t kGenerator[kSegmentECBytes + 1] = {
      146, 217, 67,  32,  75,  173, 82,  73,  220, 240,
      215, 199, 175, 149, 113, 183, 251, 239, 1,
  };

  // The error-correction bytes are the remainder of dividing |in| * x^k by
  // |kGenerator|, where |k| is the number of EC codewords. Polynomials here
  // are represented in little-endian order, i.e. the value at index |i| is
  // the coefficient of z^i.

  // Multiplication of |in| by x^k thus just involves moving it up.
  uint8_t remainder[kSegmentBytes];
  memset(remainder, 0, kSegmentECBytes);
  // Reed-Solomon input is backwards. See section 7.5.2.
  for (size_t i = 0; i < kSegmentDataBytes; i++) {
    remainder[kSegmentECBytes + i] = in[kSegmentDataBytes - 1 - i];
  }

  // Progressively eliminate the leading coefficient by subtracting some
  // multiple of |kGenerator| until we have a value smaller than |kGenerator|.
  for (size_t i = kSegmentBytes - 1; i >= kSegmentECBytes; i--) {
    // The leading coefficient of |kGenerator| is 1, so the multiple to
    // subtract to eliminate the leading term of |remainder| is the value of
    // that leading term. The polynomial ring is characteristic two, so
    // subtraction is the same as addition, which is XOR.
    for (size_t j = 0; j < sizeof(kGenerator) - 1; j++) {
      remainder[i - kSegmentECBytes + j] ^=
          GF28Mul(kGenerator[j], remainder[i]);
    }
  }

  memmove(out, in, kSegmentDataBytes);
  // Remove the Reed-Solomon remainder again to match QR's convention.
  for (size_t i = 0; i < kSegmentECBytes; i++) {
    out[kSegmentDataBytes + i] = remainder[kSegmentECBytes - 1 - i];
  }
}
