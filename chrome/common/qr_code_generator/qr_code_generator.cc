// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/qr_code_generator/qr_code_generator.h"

#include <math.h>
#include <string.h>

#include <ostream>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"

// kMaxVersionWith8BitLength is the maximum QR version that uses an 8 (rather
// than 16) bit length in 8-bit byte mode. See table 3.
static constexpr int kMaxVersionWith8BitLength = 9;

// A structure containing QR version-specific constants and data.
// All versions currently use error correction at level M.
struct QRVersionInfo {
  constexpr QRVersionInfo(const int version,
                          const uint32_t encoded_version,
                          const int size,
                          const size_t group1_bytes,
                          const size_t group1_num_blocks,
                          const size_t group1_block_data_bytes,
                          const size_t group2_bytes,
                          const size_t group2_num_blocks,
                          const size_t group2_block_data_bytes,
                          const std::array<int, 3> alignment_locations)
      : version(version),
        encoded_version(encoded_version),
        size(size),
        group1_bytes(group1_bytes),
        group1_num_blocks(group1_num_blocks),
        group1_block_data_bytes(group1_block_data_bytes),
        group2_bytes(group2_bytes),
        group2_num_blocks(group2_num_blocks),
        group2_block_data_bytes(group2_block_data_bytes),
        alignment_locations(alignment_locations) {
    if (version < 1 || version > 40 || size < 0 ||
        !CheckBlockGroupParameters(group1_bytes, group1_num_blocks,
                                   group1_block_data_bytes) ||
        (group2_num_blocks != 0 &&
         !CheckBlockGroupParameters(group2_bytes, group2_num_blocks,
                                    group2_block_data_bytes)) ||
        (version < 7 && encoded_version != 0) ||
        (version >= 7 &&
         encoded_version >> 12 != static_cast<uint32_t>(version)) ||
        (version <= kMaxVersionWith8BitLength && input_bytes() >= 256) ||
        (group2_num_blocks != 0 &&
         group2_block_ec_bytes() != group1_block_ec_bytes())) {
      __builtin_unreachable();
    }
  }

  // The version of the QR code.
  const int version;

  // An 18-bit value that contains the version, BCH (18,6)-encoded. Only valid
  // for versions seven and above. See table D.1 for values.
  const uint32_t encoded_version;

  // The number of "tiles" in each dimension for a QR code of |version|. See
  // table 1. (The colored squares in in QR codes are called tiles in the
  // spec.)
  const int size;

  // Values taken from Table 9, page 38, for a QR code of version |version|.
  const size_t group1_bytes;
  const size_t group1_num_blocks;
  const size_t group1_block_data_bytes;
  const size_t group2_bytes;
  const size_t group2_num_blocks;
  const size_t group2_block_data_bytes;

  const std::array<int, 3> alignment_locations;

  // Total number of tiles for the QR code, size*size.
  constexpr int total_size() const { return size * size; }

  constexpr size_t total_bytes() const { return group1_bytes + group2_bytes; }

  constexpr size_t group1_block_bytes() const {
    return group1_bytes / group1_num_blocks;
  }

  constexpr size_t group1_block_ec_bytes() const {
    return group1_block_bytes() - group1_block_data_bytes;
  }

  constexpr size_t group1_data_bytes() const {
    return group1_block_data_bytes * group1_num_blocks;
  }

  constexpr size_t group2_block_bytes() const {
    if (group2_num_blocks == 0)
      return 0;
    return group2_bytes / group2_num_blocks;
  }

  constexpr size_t group2_block_ec_bytes() const {
    return group2_block_bytes() - group2_block_data_bytes;
  }

  constexpr size_t group2_data_bytes() const {
    return group2_block_data_bytes * group2_num_blocks;
  }

  // Two bytes of overhead are needed for QR framing.
  // If extending beyond version 26, framing would need to be updated.
  constexpr size_t input_bytes() const {
    if (version <= 9) {
      return group1_data_bytes() + group2_data_bytes() - 2;
    } else {
      return group1_data_bytes() + group2_data_bytes() - 3;
    }
  }

 private:
  static constexpr bool CheckBlockGroupParameters(
      const size_t bytes,
      const size_t num_blocks,
      const size_t block_data_bytes) {
    if (num_blocks == 0 || bytes % num_blocks != 0 || block_data_bytes == 0 ||
        block_data_bytes * num_blocks > bytes ||
        (bytes - block_data_bytes * num_blocks) % num_blocks != 0) {
      return false;
    }

    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(QRVersionInfo);
};

namespace {

constexpr QRVersionInfo version_infos[] = {
    // See table 9 in the spec for the source of these numbers.

    // 5-M
    // 134 bytes, as 2 blocks of 67.
    {
        5,   // version
        0,   // encoded version (not included in this version)
        37,  // size (num tiles in each axis)

        // Block group 1:
        134,  // Total bytes in group
        2,    // Number of blocks
        43,   // Data bytes per block

        // Block group 2:
        0,
        0,
        0,

        // Alignment locations
        {6, 30, 0},
    },

    // 7-M
    // 196 bytes, as 4 blocks of 49.
    {
        7,                     // version
        0b000111110010010100,  // encoded version
        45,                    // size (num tiles in each axis)

        // Block group 1:
        196,  // Total bytes in group
        4,    // Number of blocks
        31,   // Data bytes per block

        // Block group 2:
        0,
        0,
        0,

        // Alignment locations
        {6, 22, 38},
    },

    // 9-M
    // 292 bytes, as 3 blocks of 58 plus 2 blocks of 59.
    {
        9,                     // version
        0b001001101010011001,  // encoded version
        53,                    // size (num tiles in each axis)

        // Block group 1:
        174,  // Total bytes in group
        3,    // Number of blocks
        36,   // Data bytes per block

        // Block group 2:
        118,
        2,
        37,

        // Alignment locations
        {6, 26, 46},
    },

    // 12-M
    // 466 bytes, as 6 blocks of 58 and 2 blocks of 59.
    {
        12,                    // version
        0b001100011101100010,  // encoded version
        65,                    // size (num tiles in each axis)

        // Block group 1:
        348,  // Total bytes in group
        6,    // Number of blocks
        36,   // Data bytes per block

        // Block group 2:
        118,
        2,
        37,

        // Alignment locations
        {6, 32, 58},
    },
};

const QRVersionInfo* GetVersionForDataSize(size_t num_data_bytes) {
  for (const auto& version : version_infos) {
    if (version.input_bytes() >= num_data_bytes) {
      return &version;
    }
  }
  return nullptr;
}

// kMaxMask is the maximum masking function number. See table 10.
constexpr uint8_t kMaxMask = 7;

// The following functions implement the masks specified in table 10.

uint8_t MaskFunction0(int x, int y) {
  return (x + y) % 2 == 0;
}
uint8_t MaskFunction1(int x, int y) {
  return y % 2 == 0;
}
uint8_t MaskFunction2(int x, int y) {
  return x % 3 == 0;
}
uint8_t MaskFunction3(int x, int y) {
  return (x + y) % 3 == 0;
}
uint8_t MaskFunction4(int x, int y) {
  return ((y / 2) + (x / 3)) % 2 == 0;
}
uint8_t MaskFunction5(int x, int y) {
  return ((x * y) % 2) + ((x * y) % 3) == 0;
}
uint8_t MaskFunction6(int x, int y) {
  return (((x * y) % 2) + ((x * y) % 3)) % 2 == 0;
}
uint8_t MaskFunction7(int x, int y) {
  return (((x + y) % 2) + ((x * y) % 3)) % 2 == 0;
}

static uint8_t (*const kMaskFunctions[kMaxMask + 1])(int x, int y) = {
    MaskFunction0, MaskFunction1, MaskFunction2, MaskFunction3,
    MaskFunction4, MaskFunction5, MaskFunction6, MaskFunction7,
};

// kFormatInformation is taken from table C.1 on page 80 and specifies the
// format value for each masking function, assuming ECC level 'M'.
static const uint16_t kFormatInformation[kMaxMask + 1] = {
    0x5412, 0x5125, 0x5e7c, 0x5b4b, 0x45f9, 0x40ce, 0x4f97, 0x4aa0,
};

}  // namespace

QRCodeGenerator::QRCodeGenerator() = default;

QRCodeGenerator::~QRCodeGenerator() = default;

QRCodeGenerator::GeneratedCode::GeneratedCode() = default;
QRCodeGenerator::GeneratedCode::GeneratedCode(
    QRCodeGenerator::GeneratedCode&&) = default;
QRCodeGenerator::GeneratedCode::~GeneratedCode() = default;

base::Optional<QRCodeGenerator::GeneratedCode> QRCodeGenerator::Generate(
    base::span<const uint8_t> in,
    base::Optional<uint8_t> mask) {
  CHECK(!mask || *mask <= kMaxMask);

  // We're currently using a minimal set of versions to shrink test surface.
  // When expanding, take care to validate across different platforms and
  // a selection of QR Scanner apps.
  const QRVersionInfo* const version_info = GetVersionForDataSize(in.size());
  if (!version_info) {
    return base::nullopt;
  }

  if (version_info != version_info_) {
    version_info_ = version_info;
    d_.resize(version_info_->total_size());
  }
  // Previous data and "set" bits must be cleared.
  memset(&d_[0], 0, version_info_->total_size());

  PutVerticalTiming(6);
  PutHorizontalTiming(6);
  PutFinder(3, 3);
  PutFinder(3, version_info_->size - 4);
  PutFinder(version_info_->size - 4, 3);

  const auto& alignment_locations = version_info_->alignment_locations;
  size_t num_alignment_locations = 0;
  for (size_t i = 0; i < alignment_locations.size(); i++) {
    if (alignment_locations[i] == 0) {
      break;
    }
    num_alignment_locations++;
  }

  for (size_t i = 0; i < num_alignment_locations; i++) {
    for (size_t j = 0; j < num_alignment_locations; j++) {
      // Three of the corners already have finder symbols.
      if ((i == 0 && j == 0) || (i == 0 && j == num_alignment_locations - 1) ||
          (i == num_alignment_locations - 1 && j == 0)) {
        continue;
      }

      PutAlignment(alignment_locations[i], alignment_locations[j]);
    }
  }

  if (version_info_->encoded_version != 0) {
    PutVersionBlocks(version_info_->encoded_version);
  }

  // Add the mode and character count.

  // QR codes require some framing of the data. This requires:
  // Version 1-9:   4 bits for mode + 8 bits for char count = 12 bits
  // Version 10-40: 4 bits for mode + 16 bits for char count = 20 bits
  // Details are in Table 3.
  // Since 12 and 20 are not a multiple of eight, a frame-shift of all
  // subsequent bytes is required.
  const size_t framed_input_size =
      version_info_->group1_data_bytes() + version_info_->group2_data_bytes();
  std::vector<uint8_t> prefixed_data(framed_input_size);
  size_t framing_offset_bytes = 0;
  if (version_info->version <= kMaxVersionWith8BitLength) {
    DCHECK_LT(in.size(), 0x100u) << "in.size() too large for 8-bit length";
    const uint8_t len8 = static_cast<uint8_t>(in.size());
    prefixed_data[0] = 0x40 | (len8 >> 4);
    prefixed_data[1] = len8 << 4;
    if (!in.empty()) {
      prefixed_data[1] |= in[0] >> 4;
    }
    framing_offset_bytes = 2;
  } else {
    DCHECK_LT(in.size(), 0x10000u) << "in.size() too large for 16-bit length";
    const uint16_t len16 = static_cast<uint16_t>(in.size());
    prefixed_data[0] = 0x40 | (len16 >> 12);
    prefixed_data[1] = len16 >> 4;
    prefixed_data[2] = len16 << 4;
    if (!in.empty()) {
      prefixed_data[2] |= in[0] >> 4;
    }
    framing_offset_bytes = 3;
  }
  DCHECK_LE(in.size() + framing_offset_bytes, prefixed_data.size());

  for (size_t i = 0; i < in.size() - 1; i++) {
    prefixed_data[i + framing_offset_bytes] = (in[i] << 4) | (in[i + 1] >> 4);
  }
  if (!in.empty()) {
    prefixed_data[in.size() - 1 + framing_offset_bytes] = in[in.size() - 1]
                                                          << 4;
  }

  // The QR code looks a little odd with fixed padding. Thus replicate the
  // message to fill the input.
  for (size_t i = in.size() + framing_offset_bytes; i < framed_input_size;
       i++) {
    prefixed_data[i] = prefixed_data[i % (in.size() + framing_offset_bytes)];
  }

  // Each block of input data is expanded with error correcting
  // information and then interleaved.

  // Error Correction for Group 1, present for all versions.
  const size_t group1_num_blocks = version_info_->group1_num_blocks;
  const size_t group1_block_bytes = version_info_->group1_block_bytes();
  const size_t group1_block_data_bytes = version_info_->group1_block_data_bytes;
  const size_t group1_block_ec_bytes = version_info_->group1_block_ec_bytes();
  std::vector<std::vector<uint8_t>> expanded_blocks(group1_num_blocks);
  for (size_t i = 0; i < group1_num_blocks; i++) {
    expanded_blocks[i].resize(group1_block_bytes);
    AddErrorCorrection(
        expanded_blocks[i],
        base::span<const uint8_t>(&prefixed_data[group1_block_data_bytes * i],
                                  group1_block_data_bytes),
        group1_block_bytes, group1_block_ec_bytes);
  }

  // Error Correction for Group 2, present for some versions.
  // Factor out the number of bytes written by the prior group.
  const size_t group_data_offset = version_info_->group1_data_bytes();
  const size_t group2_num_blocks = version_info_->group2_num_blocks;
  const size_t group2_block_bytes = version_info_->group2_block_bytes();
  const size_t group2_block_data_bytes = version_info_->group2_block_data_bytes;
  const size_t group2_block_ec_bytes = version_info_->group2_block_ec_bytes();

  std::vector<std::vector<uint8_t>> expanded_blocks_2;
  if (group2_num_blocks > 0) {
    expanded_blocks_2.resize(group2_num_blocks);
    for (size_t i = 0; i < group2_num_blocks; i++) {
      expanded_blocks_2[i].resize(group2_block_bytes);
      AddErrorCorrection(
          expanded_blocks_2[i],
          base::span<const uint8_t>(
              &prefixed_data[group_data_offset + group2_block_data_bytes * i],
              group2_block_data_bytes),
          group2_block_bytes, group2_block_ec_bytes);
    }
  }

  const size_t total_bytes = version_info_->total_bytes();
  uint8_t interleaved_data[total_bytes];
  CHECK(total_bytes == group1_block_bytes * group1_num_blocks +
                           group2_block_bytes * group2_num_blocks)
      << "internal error";

  size_t k = 0;
  // Interleave data from all blocks. All non-ECC data is written before any ECC
  // data. The group two blocks, if any, will be longer than the group one
  // blocks. Once group one is exhausted then the interleave considers only
  // group two.
  DCHECK(group2_num_blocks == 0 || version_info_->group2_block_data_bytes >
                                       version_info_->group1_block_data_bytes);

  size_t j = 0;
  for (; j < version_info_->group1_block_data_bytes; j++) {
    for (size_t i = 0; i < group1_num_blocks; i++) {
      interleaved_data[k++] = expanded_blocks[i][j];
    }
    for (size_t i = 0; i < group2_num_blocks; i++) {
      interleaved_data[k++] = expanded_blocks_2[i][j];
    }
  }
  if (group2_num_blocks > 0) {
    for (; j < version_info_->group2_block_data_bytes; j++) {
      for (size_t i = 0; i < group2_num_blocks; i++) {
        interleaved_data[k++] = expanded_blocks_2[i][j];
      }
    }
  }

  // The number of ECC bytes in each group is the same so the interleave
  // considers them uniformly.
  DCHECK(version_info_->group2_num_blocks == 0 ||
         version_info_->group2_block_ec_bytes() ==
             version_info_->group1_block_ec_bytes());
  for (size_t j = 0; j < version_info_->group1_block_ec_bytes(); j++) {
    for (size_t i = 0; i < group1_num_blocks; i++) {
      interleaved_data[k++] =
          expanded_blocks[i][version_info_->group1_block_data_bytes + j];
    }
    for (size_t i = 0; i < group2_num_blocks; i++) {
      interleaved_data[k++] =
          expanded_blocks_2[i][version_info_->group2_block_data_bytes + j];
    }
  }
  DCHECK_EQ(k, total_bytes);

  uint8_t best_mask = mask.value_or(0);
  base::Optional<unsigned> lowest_penalty;

  // If |mask| was not specified, then evaluate each masking function to find
  // the one with the lowest penalty score.
  for (uint8_t mask_num = 0; !mask && mask_num <= kMaxMask; mask_num++) {
    // kFormatInformation is the encoded formatting word for the QR code that
    // this code generates. See tables 10 and 12. For example:
    //                  00 011
    //                  --|---
    // error correction M | Mask pattern 3
    //
    // It's translated into a 15-bit value using the table on page 80, which is
    // stored in |kFormatInformation|.
    PutFormatBits(kFormatInformation[mask_num]);

    PutBits(interleaved_data, sizeof(interleaved_data),
            kMaskFunctions[mask_num]);

    const unsigned penalty = CountPenaltyPoints();
    if (!lowest_penalty || *lowest_penalty > penalty) {
      lowest_penalty = penalty;
      best_mask = mask_num;
    }
  }

  // Repaint with the best mask function.
  PutFormatBits(kFormatInformation[best_mask]);
  PutBits(interleaved_data, sizeof(interleaved_data),
          kMaskFunctions[best_mask]);

  GeneratedCode code;
  code.data = base::span<uint8_t>(&d_[0], version_info_->total_size());
  code.qr_size = version_info_->size;
  return code;
}

// PutFinder paints a finder symbol at the given coordinates.
void QRCodeGenerator::PutFinder(int x, int y) {
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
void QRCodeGenerator::PutAlignment(int x, int y) {
  fillAt(x - 2, y - 2, 5, 0b11);
  fillAt(x - 2, y + 2, 5, 0b11);
  static constexpr uint8_t kLine[5] = {0b11, 0b10, 0b10, 0b10, 0b11};
  copyTo(x - 2, y - 1, kLine, sizeof(kLine));
  copyTo(x - 2, y, kLine, sizeof(kLine));
  copyTo(x - 2, y + 1, kLine, sizeof(kLine));
  at(x, y) = 0b11;
}

// PutVerticalTiming paints the vertical timing signal.
void QRCodeGenerator::PutVerticalTiming(int x) {
  for (int y = 0; y < version_info_->size; y++) {
    at(x, y) = 0b10 | (1 ^ (y & 1));
  }
}

// PutVerticalTiming paints the horizontal timing signal.
void QRCodeGenerator::PutHorizontalTiming(int y) {
  for (int x = 0; x < version_info_->size; x++) {
    at(x, y) = 0b10 | (1 ^ (x & 1));
  }
}

// PutFormatBits paints the 15-bit, pre-encoded format metadata. See page 56
// for the location of the format bits.
void QRCodeGenerator::PutFormatBits(const uint16_t format) {
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

  const int size = version_info_->size;
  v = format;
  for (int x = size - 1; x >= size - 1 - 7; x--) {
    at(x, 8) = 0b10 | (v & 1);
    v >>= 1;
  }

  at(8, size - 1 - 7) = 0b11;
  for (int y = size - 1 - 6; y <= size - 1; y++) {
    at(8, y) = 0b10 | (v & 1);
    v >>= 1;
  }
}

void QRCodeGenerator::PutVersionBlocks(uint32_t encoded_version) {
  // Version 7 and larger require 18-bit version information taking the form
  // of 6x3 rectangles above the bottom-left locator and to the left of the
  // top-right locator.
  const int size = version_info_->size;

  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 3; j++) {
      // Bottom-left rectangle is top-to-bottom, left-to-right
      at(i, size - 8 - 3 + j) = 0b10 | (encoded_version & 1);
      // Top-right rectangle is left-to-right, top-to-bottom
      at(size - 8 - 3 + j, i) = 0b10 | (encoded_version & 1);
      // Shift to consider the next bit.
      encoded_version >>= 1;
    }
  }
}

// PutBits writes the given data into the QR code in correct order, avoiding
// structural elements that must have already been painted. See section 7.7.3
// about the placement algorithm.
void QRCodeGenerator::PutBits(const uint8_t* data,
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
  int x = version_info_->size - 1;
  int y = version_info_->size - 1;

  for (;;) {
    uint8_t& right = at(x, y);
    // Test the current value in the QR code to avoid painting over any
    // existing structural elements.
    if ((right & 2) == 0) {
      right = stream.Next() ^ mask_func(x, y);
    }

    uint8_t& left = at(x - 1, y);
    if ((left & 2) == 0) {
      left = stream.Next() ^ mask_func(x - 1, y);
    }

    if ((going_up && y == 0) || (!going_up && y == version_info_->size - 1)) {
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
uint8_t& QRCodeGenerator::at(int x, int y) {
  DCHECK_LE(0, x);
  DCHECK_LT(x, version_info_->size);
  DCHECK_LE(0, y);
  DCHECK_LT(y, version_info_->size);
  return d_[version_info_->size * y + x];
}

// fillAt sets the |len| elements at (x, y) to |value|.
void QRCodeGenerator::fillAt(int x, int y, size_t len, uint8_t value) {
  DCHECK_LE(0, x);
  DCHECK_LE(static_cast<int>(x + len), version_info_->size);
  DCHECK_LE(0, y);
  DCHECK_LT(y, version_info_->size);
  memset(&d_[version_info_->size * y + x], value, len);
}

// copyTo copies |len| elements from |data| to the elements at (x, y).
void QRCodeGenerator::copyTo(int x, int y, const uint8_t* data, size_t len) {
  DCHECK_LE(0, x);
  DCHECK_LE(static_cast<int>(x + len), version_info_->size);
  DCHECK_LE(0, y);
  DCHECK_LT(y, version_info_->size);
  memcpy(&d_[version_info_->size * y + x], data, len);
}

// clipped returns a reference to the given element of |d_|, or to
// |clip_dump_| if the coordinates are out of bounds.
uint8_t& QRCodeGenerator::clipped(int x, int y) {
  if (0 <= x && x < version_info_->size && 0 <= y && y < version_info_->size) {
    return d_[version_info_->size * y + x];
  }
  return clip_dump_;
}

// GF28Mul returns the product of |a| and |b| (which must be field elements,
// i.e. < 256) in the field GF(2^8) mod x^8 + x^4 + x^3 + x^2 + 1.
// static
uint8_t QRCodeGenerator::GF28Mul(uint16_t a, uint16_t b) {
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
// |out| should have length block_bytes for the code's version.
// |in| should have length block_data_bytes for the code's version.
void QRCodeGenerator::AddErrorCorrection(base::span<uint8_t> out,
                                         base::span<const uint8_t> in,
                                         size_t block_bytes,
                                         size_t block_ec_bytes) {
  // kGenerator is the product of (z - x^i) for 0 <= i < |block_ec_bytes|,
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
  // gen = generatorPoly(24)
  // coeffs = list(gen)
  // gen = [toByte(x) for x in coeffs]
  // print 'uint8_t kGenerator[' + str(len(gen)) + '] = {' + str(gen) + '}'

  // Used for 7-M: 18 error correction codewords per block.
  static const uint8_t kGenerator18[] = {
      146, 217, 67,  32,  75,  173, 82,  73,  220, 240,
      215, 199, 175, 149, 113, 183, 251, 239, 1,
  };

  // Used for 9-M and 12-M; 22 error correction codewords per block.
  static const uint8_t kGenerator22[] = {
      245, 145, 26,  230, 218, 86,  253, 67,  123, 29, 137, 28,
      40,  69,  189, 19,  244, 182, 176, 131, 179, 89, 1,
  };

  // Used for 5-M, 24 error correction codewords per block.
  static const uint8_t kGenerator24[] = {
      117, 144, 217, 127, 247, 237, 1,   206, 43,  61,  72,  130, 73,
      229, 150, 115, 102, 216, 237, 178, 70,  169, 118, 122, 1,
  };

  const uint8_t* generator = nullptr;
  switch (block_ec_bytes) {
    case 18:
      generator = kGenerator18;
      break;
    case 22:
      generator = kGenerator22;
      break;
    case 24:
      generator = kGenerator24;
      break;
    default: {
      NOTREACHED() << "Unsupported Generator Polynomial for block_ec_bytes: "
                   << block_ec_bytes;
      return;
    }
  }

  // The error-correction bytes are the remainder of dividing |in| * x^k by
  // |kGenerator|, where |k| is the number of EC codewords. Polynomials here
  // are represented in little-endian order, i.e. the value at index |i| is
  // the coefficient of z^i.

  // Multiplication of |in| by x^k thus just involves moving it up.
  uint8_t remainder[block_bytes];
  DCHECK_LE(block_ec_bytes, block_bytes);
  memset(remainder, 0, block_ec_bytes);
  size_t block_data_bytes = block_bytes - block_ec_bytes;
  // Reed-Solomon input is backwards. See section 7.5.2.
  for (size_t i = 0; i < block_data_bytes; i++) {
    remainder[block_ec_bytes + i] = in[block_data_bytes - 1 - i];
  }

  // Progressively eliminate the leading coefficient by subtracting some
  // multiple of |generator| until we have a value smaller than |generator|.
  for (size_t i = block_bytes - 1; i >= block_ec_bytes; i--) {
    // The leading coefficient of |generator| is 1, so the multiple to
    // subtract to eliminate the leading term of |remainder| is the value of
    // that leading term. The polynomial ring is characteristic two, so
    // subtraction is the same as addition, which is XOR.
    for (size_t j = 0; j < block_ec_bytes; j++) {
      remainder[i - block_ec_bytes + j] ^= GF28Mul(generator[j], remainder[i]);
    }
  }

  memmove(&out[0], &in[0], block_data_bytes);
  // Remove the Reed-Solomon remainder again to match QR's convention.
  for (size_t i = 0; i < block_ec_bytes; i++) {
    out[block_data_bytes + i] = remainder[block_ec_bytes - 1 - i];
  }
}

unsigned QRCodeGenerator::CountPenaltyPoints() const {
  const int size = version_info_->size;
  unsigned penalty = 0;

  // The spec penalises the pattern X.XXX.X with four unpainted tiles to
  // the left or right. These are "finder-like" patterns. To catch them, a
  // sliding window of 11 tiles is used.
  static const unsigned k11Bits = 0x7ff;
  static const unsigned kFinderLeft = 0b00001011101;
  static const unsigned kFinderRight = 0b10111010000;

  // Count:
  //   * Horizontal runs of the same color, at least five tiles in a row.
  //   * The number of horizontal finder-like patterns.
  //   * Total number of painted tiles, which is used later.
  unsigned current_run_length;
  int current_color;
  unsigned total_painted_tiles = 0;
  unsigned window = 0;

  size_t i = 0;
  for (int y = 0; y < size; y++) {
    current_color = d_[i++] & 1;
    current_run_length = 0;
    window = current_color;
    total_painted_tiles += current_color;

    for (int x = 1; x < size; x++) {
      const int color = d_[i++] & 1;

      window = k11Bits & ((window << 1) | color);
      if (window == kFinderLeft || window == kFinderRight) {
        penalty += 40;
      }

      total_painted_tiles += color;

      if (color == current_color) {
        current_run_length++;
        continue;
      }

      if (current_run_length >= 5) {
        penalty += current_run_length - 2;
      }
      current_run_length = 0;
      current_color = color;
    }

    if (current_run_length >= 5) {
      penalty += current_run_length - 2;
    }

    window = k11Bits & (window << 4);
    if (window == kFinderRight) {
      penalty += 40;
    }
  }
  DCHECK_EQ(i, static_cast<size_t>(size * size));

  // Count:
  //   * Vertical runs of the same color, at least five tiles in a row.
  //   * The number of vertical finder-like patterns.
  for (int x = 0; x < size; x++) {
    i = x;
    current_run_length = 0;
    current_color = d_[i] & 1;
    i += size;
    window = current_color;

    for (int y = 1; y < size; y++, i += size) {
      const int color = d_[i] & 1;
      window = k11Bits & ((window << 1) | color);
      if (window == kFinderLeft || window == kFinderRight) {
        penalty += 40;
      }

      if (color == current_color) {
        current_run_length++;
        continue;
      }

      if (current_run_length >= 5) {
        penalty += current_run_length - 2;
      }
      current_run_length = 0;
      current_color = color;
    }

    if (current_run_length >= 5) {
      penalty += current_run_length - 2;
    }

    window = k11Bits & (window << 4);
    if (window == kFinderRight) {
      penalty += 40;
    }
  }
  DCHECK_EQ(i, static_cast<size_t>(size * size + size - 1));

  // Count 2x2 blocks of the same color.
  i = 0;
  for (int y = 0; y < size - 1; y++) {
    for (int x = 0; x < size - 1; x++) {
      const int color = d_[i++] & 1;
      if ((d_[i + 1] & 1) == color && (d_[i + size] & 1) == color &&
          (d_[i + size + 1] & 1) == color) {
        penalty += 3;
      }
    }
  }

  // Each deviation of 5% away from 50%-painted costs five points.
  DCHECK_LE(total_painted_tiles, static_cast<unsigned>(size) * size);
  double painted_fraction = static_cast<double>(total_painted_tiles) /
                            (static_cast<double>(size) * size);
  if (painted_fraction < 0.5) {
    painted_fraction = 1.0 - painted_fraction;
  }
  const double deviation = (painted_fraction - 0.5) / 0.05;
  penalty += 5 * static_cast<unsigned>(floor(deviation));

  return penalty;
}
