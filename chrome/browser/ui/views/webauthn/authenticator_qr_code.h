// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_CODE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_CODE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"

// AuthenticatorQRCode generates version three, class Q QR codes that carry 32
// bytes of raw data. References in the following comments refer to ISO 18004
// (3rd edition).
class AuthenticatorQRCode {
 public:
  // kSize is the number of "tiles" in each dimension for a v3 QR code. See
  // table 1. (The colored squares in in QR codes are called tiles in the
  // spec.)
  static constexpr int kSize = 29;
  // kTotalSize is the total number of tiles for a v3 QR code, in both
  // directions.
  static constexpr int kTotalSize = kSize * kSize;
  // These values are taken from table 9 (page 38) for a version three, class Q
  // QR code.
  static constexpr size_t kTotalBytes = 70;
  static constexpr size_t kNumSegments = 2;
  static constexpr size_t kSegmentDataBytes = 17;

  static constexpr size_t kSegmentBytes = kTotalBytes / kNumSegments;
  static constexpr size_t kSegmentECBytes = kSegmentBytes - kSegmentDataBytes;
  static constexpr size_t kDataBytes = kSegmentDataBytes * kNumSegments;
  // Two bytes of overhead are needed for QR framing.
  static constexpr size_t kInputBytes = kDataBytes - 2;

  // Generate generates a QR code containing the given data and returns a
  // pointer to an array of kTotalSize bytes where the least-significant bit of
  // each byte is set if that tile should be "black".
  base::span<const uint8_t, kTotalSize> Generate(const uint8_t in[kInputBytes]);

 private:
  // MaskFunction3 implements one of the data-masking functions. See figure 21.
  static uint8_t MaskFunction3(int x, int y);

  // PutFinder paints a finder symbol at the given coordinates.
  void PutFinder(int x, int y);

  // PutAlignment paints an alignment symbol centered at the given coordinates.
  void PutAlignment(int x, int y);

  // PutVerticalTiming paints the vertical timing signal.
  void PutVerticalTiming(int x);

  // PutVerticalTiming paints the horizontal timing signal.
  void PutHorizontalTiming(int y);

  // PutFormatBits paints the 15-bit, pre-encoded format metadata. See page 56
  // for the location of the format bits.
  void PutFormatBits(const uint16_t format);

  // PutBits writes the given data into the QR code in correct order, avoiding
  // structural elements that must have already been painted. See section 7.7.3
  // about the placement algorithm.
  void PutBits(const uint8_t* data,
               size_t data_len,
               uint8_t (*mask_func)(int, int));

  // at returns a reference to the given element of |d_|.
  uint8_t& at(int x, int y);

  // fillAt sets the |len| elements at (x, y) to |value|.
  void fillAt(int x, int y, size_t len, uint8_t value);

  // copyTo copies |len| elements from |data| to the elements at (x, y).
  void copyTo(int x, int y, const uint8_t* data, size_t len);

  // clipped returns a reference to the given element of |d_|, or to
  // |clip_dump_| if the coordinates are out of bounds.
  uint8_t& clipped(int x, int y);

  // GF28Mul returns the product of |a| and |b| (which must be field elements,
  // i.e. < 256) in the field GF(2^8) mod x^8 + x^4 + x^3 + x^2 + 1.
  static uint8_t GF28Mul(uint16_t a, uint16_t b);

  // AddErrorCorrection writes the Reed-Solomon expanded version of |in| to
  // |out|.
  static void AddErrorCorrection(uint8_t out[kSegmentBytes],
                                 const uint8_t in[kSegmentDataBytes]);

  // d_ represents a QR code with one byte per pixel. Each byte is one pixel.
  // The LSB is set if the pixel is "black". The second bit is set if the pixel
  // is part of the structure of the QR code, i.e. finder or alignment symbols,
  // timing patterns, or format data.
  uint8_t d_[kSize * kSize];
  // clip_dump_ is the target of paints that would otherwise fall outside of the
  // QR code.
  uint8_t clip_dump_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_AUTHENTICATOR_QR_CODE_H_
