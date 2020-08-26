// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_
#define CHROME_COMMON_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/optional.h"

struct QRVersionInfo;

// QRCodeGenerator generates class M QR codes of various versions.
// References in the following comments refer to ISO 18004 (3rd edition).
// Supports versions up to 26 by adding constants.
class QRCodeGenerator {
 public:
  // Contains output data for Generate().
  // The default state contains no data.
  struct GeneratedCode {
   public:
    GeneratedCode();
    GeneratedCode(GeneratedCode&&);
    ~GeneratedCode();

    // Pixel data; pointer to an array of bytes, where the least-significant
    // bit of each byte is set if that tile should be "black".
    // Clients should ensure four modules of padding when rendering the code.
    // On error, will not be populated, and will evaluate to false.
    base::span<uint8_t> data;

    // Width and height (which are equal) of the generated data, in tiles.
    int qr_size = 0;

    DISALLOW_COPY_AND_ASSIGN(GeneratedCode);
  };

  // Static parameters for V5 QR codes.
  // These exist while migrating clients to dynamic sizes.
  struct V5 {
    static constexpr int kSize = 37;
    static constexpr int kTotalSize = kSize * kSize;
    static constexpr size_t kNumSegments = 2;
    static constexpr size_t kSegmentDataBytes = 43;
    static constexpr size_t kDataBytes = kSegmentDataBytes * kNumSegments;
    static constexpr size_t kInputBytes = kDataBytes - 2;
  };

  QRCodeGenerator();
  ~QRCodeGenerator();

  // Generates a QR code containing the given data.
  // The generator will attempt to choose a version that fits the data. The
  // returned span's length is input-dependent and not known at compile-time in
  // this case. The optional |mask| argument specifies the QR mask value to use
  // (from 0 to 7). If not specified, the optimal mask is calculated per the
  // algorithm specified in the QR standard.
  base::Optional<GeneratedCode> Generate(
      base::span<const uint8_t> in,
      base::Optional<uint8_t> mask = base::nullopt);

 private:
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

  // PutVersionBlocks writes the two blocks of version information for QR
  // versions seven and above.
  void PutVersionBlocks(const uint32_t encoded_version);

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
  // |out| should have length block_bytes for the code's version.
  // |in| should have length block_data_bytes for the code's version.
  // |block_bytes| and |block_ec_bytes| must be provided for the current
  // version/level/group.
  void AddErrorCorrection(base::span<uint8_t> out,
                          base::span<const uint8_t> in,
                          size_t block_bytes,
                          size_t block_ec_bytes);

  // CountPenaltyPoints sums the penalty points for the current, fully drawn,
  // code. See table 11.
  unsigned CountPenaltyPoints() const;

  // Parameters for the currently-selected version of the QR code.
  // Generate() will pick a version that can contain enough data.
  // Unowned; nullptr until initialized in Generate().
  const QRVersionInfo* version_info_ = nullptr;

  // d_ represents a QR code with one byte per pixel. Each byte is one pixel.
  // The LSB is set if the pixel is "black". The second bit is set if the pixel
  // is part of the structure of the QR code, i.e. finder or alignment symbols,
  // timing patterns, or format data.
  // Initialized and possibly reinitialized in Generate().
  std::vector<uint8_t> d_;

  // clip_dump_ is the target of paints that would otherwise fall outside of the
  // QR code.
  uint8_t clip_dump_;
};

#endif  // CHROME_COMMON_QR_CODE_GENERATOR_QR_CODE_GENERATOR_H_
