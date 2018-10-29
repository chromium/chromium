// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/randomized_encoder.h"

#include <algorithm>
#include <limits>

#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/hkdf.h"

namespace autofill {

struct RandomizedEncoder::EncodingInfo {
  AutofillRandomizedValue_EncodingType encoding_type;
  size_t final_size;
  size_t bit_offset;
  size_t bit_stride;
};

namespace {

const RandomizedEncoder::EncodingInfo kEncodingInfo[] = {
    // One bit per byte. These all require 8 bytes to encode and have 8-bit
    // strides, starting from a different initial bit offset.
    {AutofillRandomizedValue_EncodingType_BIT_0, 8, 0, 8},
    {AutofillRandomizedValue_EncodingType_BIT_1, 8, 1, 8},
    {AutofillRandomizedValue_EncodingType_BIT_2, 8, 2, 8},
    {AutofillRandomizedValue_EncodingType_BIT_3, 8, 3, 8},
    {AutofillRandomizedValue_EncodingType_BIT_4, 8, 4, 8},
    {AutofillRandomizedValue_EncodingType_BIT_5, 8, 5, 8},
    {AutofillRandomizedValue_EncodingType_BIT_6, 8, 6, 8},
    {AutofillRandomizedValue_EncodingType_BIT_7, 8, 7, 8},

    // Four bits per byte. These require 32 bytes to encode and have 2-bit
    // strides/
    {AutofillRandomizedValue_EncodingType_EVEN_BITS, 32, 0, 2},
    {AutofillRandomizedValue_EncodingType_ODD_BITS, 32, 1, 2},

    // All bits per byte. This require 64 bytes to encode and has a 1-bit
    // stride.
    {AutofillRandomizedValue_EncodingType_ALL_BITS, 64, 0, 1},
};

// Size related constants.
constexpr size_t kBitsPerByte = 8;
constexpr size_t kMaxEncodedLengthInBytes = 64;
constexpr size_t kMaxEncodedLengthInBits =
    kMaxEncodedLengthInBytes * kBitsPerByte;

// Find the EncodingInfo struct for |encoding_type|, else return nullptr.
const RandomizedEncoder::EncodingInfo* GetEncodingInfo(
    AutofillRandomizedValue_EncodingType encoding_type) {
  DCHECK(std::is_sorted(std::begin(kEncodingInfo), std::end(kEncodingInfo),
                        [](const RandomizedEncoder::EncodingInfo& lhs,
                           const RandomizedEncoder::EncodingInfo& rhs) {
                          return lhs.encoding_type < rhs.encoding_type;
                        }));

  const auto* encode_info = std::lower_bound(
      std::begin(kEncodingInfo), std::end(kEncodingInfo), encoding_type,
      [](const RandomizedEncoder::EncodingInfo& lhs,
         AutofillRandomizedValue_EncodingType encoding_type) {
        return lhs.encoding_type < encoding_type;
      });

  return (encode_info != std::end(kEncodingInfo) &&
          encode_info->encoding_type == encoding_type)
             ? encode_info
             : nullptr;
}

// Get the |i|-th bit of |s| where |i| counts up from the 0-bit of the first
// character in |s|. It is expected that the caller guarantees that |i| is a
// valid bit-offset into |s|
inline uint8_t GetBit(base::StringPiece s, size_t i) {
  DCHECK_LT(i / kBitsPerByte, s.length());
  return static_cast<bool>((s[i / kBitsPerByte]) & (1 << (i % kBitsPerByte)));
}

// Set the |i|-th bit of |s| where |i| counts up from the 0-bit of the first
// character in |s|. It is expected that the caller guarantees that |i| is a
// valid bit-offset into |s|.
inline void SetBit(size_t i, uint8_t bit_value, std::string* s) {
  DCHECK(bit_value == 0u || bit_value == 1u);
  DCHECK(s);
  DCHECK_LT(i / kBitsPerByte, s->length());

  // Clear the target bit value.
  (*s)[i / kBitsPerByte] &= ~(1 << (i % kBitsPerByte));

  // Set the target bit to the input bit-value.
  (*s)[i / kBitsPerByte] |= (bit_value << (i % kBitsPerByte));
}
// Returns a pseudo-random value of length |kMaxEncodedLengthInBytes| that is
// derived from |secret|, |purpose|, |form_signature|, |field_signature| and
// |data_type|.
std::string GetPseudoRandomBits(base::StringPiece secret,
                                base::StringPiece purpose,
                                FormSignature form_signature,
                                FieldSignature field_signature,
                                base::StringPiece data_type) {
  // The purpose and data_type strings are expect to be small semantic
  // identifiers: "noise", "coins", "css_class", "html-name", "html_id", etc.
  int purpose_length = base::checked_cast<int>(purpose.length());
  int data_type_length = base::checked_cast<int>(data_type.length());

  // Join the descriptive information about the encoding about to be performed.
  std::string info =
      base::StringPrintf("%d:%.*s;%08" PRIx64 ";%08" PRIx64 ";%d:%.*s",
                         purpose_length, purpose_length, purpose.data(),
                         form_signature, static_cast<uint64_t>(field_signature),
                         data_type_length, data_type_length, data_type.data());

  DVLOG(1) << "Generating pseudo-random bits from " << info;

  // Generate the pseudo-random bits.
  return crypto::HkdfSha256(secret, {}, info, kMaxEncodedLengthInBytes);
}

}  // namespace

RandomizedEncoder::RandomizedEncoder(
    std::string seed,
    AutofillRandomizedValue_EncodingType encoding_type)
    : seed_(std::move(seed)), encoding_info_(GetEncodingInfo(encoding_type)) {
  DCHECK(encoding_info_ != nullptr);
}

std::string RandomizedEncoder::Encode(FormSignature form_signature,
                                      FieldSignature field_signature,
                                      base::StringPiece data_type,
                                      base::StringPiece data_value) const {
  if (!encoding_info_) {
    NOTREACHED();
    return std::string();
  }

  std::string coins = GetCoins(form_signature, field_signature, data_type);
  std::string noise = GetNoise(form_signature, field_signature, data_type);

  DCHECK_EQ(kMaxEncodedLengthInBytes, coins.length());
  DCHECK_EQ(kMaxEncodedLengthInBytes, noise.length());

  // If we're encoding the bits encoding we can simply repurpose the noise
  // vector and use the coins vector merge in the selected data value bits.
  // For each bit, the encoded value is the true value if the coin-toss is TRUE
  // or the noise value if the coin-toss is FALSE. All the bits in a given byte
  // can be computed in parallel. The trailing bytes are all noise.
  if (encoding_info_->encoding_type ==
      AutofillRandomizedValue_EncodingType_ALL_BITS) {
    std::string all_bits = std::move(noise);
    const size_t value_length =
        std::min(data_value.length(), kMaxEncodedLengthInBytes);
    for (size_t i = 0; i < value_length; ++i) {
      // Initially this byte is all noise, we're replacing the bits for which
      // the coin toss is 1 with the corresponding data_value bits, and keeping
      // the noise bits where the coin toss is 0.
      all_bits[i] = (data_value[i] & coins[i]) | (all_bits[i] & ~coins[i]);
    }
    return all_bits;
  }

  // Otherwise, pack the select the subset of bits into an output buffer.
  // This encodes every |encoding_info_->bit_stride| bit starting from
  // |encoding_info_->bit_offset|.
  //
  // For each bit, the encoded value is the true value if the coin-toss is TRUE
  // or the noise value if the coin-toss is FALSE. All the bits in a given byte
  // can be computed in parallel. The trailing bytes are all noise.
  std::string output(encoding_info_->final_size, 0);
  const size_t value_length_in_bits = data_value.length() * kBitsPerByte;
  size_t dst_offset = 0;
  size_t src_offset = encoding_info_->bit_offset;
  while (src_offset < kMaxEncodedLengthInBits) {
    uint8_t output_bit = GetBit(noise, src_offset);
    if (src_offset < value_length_in_bits) {
      const uint8_t coin_bit = GetBit(coins, src_offset);
      const uint8_t data_bit = GetBit(data_value, src_offset);
      output_bit = ((coin_bit & data_bit) | (~coin_bit & output_bit));
    }
    SetBit(dst_offset, output_bit, &output);
    src_offset += encoding_info_->bit_stride;
    dst_offset += 1;
  }

  DCHECK_EQ(dst_offset, encoding_info_->final_size * kBitsPerByte);
  return output;
}

std::string RandomizedEncoder::GetCoins(FormSignature form_signature,
                                        FieldSignature field_signature,
                                        base::StringPiece data_type) const {
  return GetPseudoRandomBits(seed_, "coins", form_signature, field_signature,
                             data_type);
}

// Get the pseudo-random string to use at the noise bit-field.
std::string RandomizedEncoder::GetNoise(FormSignature form_signature,
                                        FieldSignature field_signature,
                                        base::StringPiece data_type) const {
  return GetPseudoRandomBits(seed_, "noise", form_signature, field_signature,
                             data_type);
}

}  // namespace autofill
