// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/randomized_encoder.h"

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>

#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/signatures.h"
#include "components/prefs/pref_service.h"
#include "crypto/hkdf.h"

namespace autofill {

namespace {

// Size related constants.
constexpr size_t kBitsPerByte = 8;
constexpr size_t kEncodedChunkLengthInBytes = 64;
constexpr size_t kMaxChunks = 8;

// Get the |i|-th bit of |s| where |i| counts up from the 0-bit of the first
// character in |s|. It is expected that the caller guarantees that |i| is a
// valid bit-offset into |s|
inline uint8_t GetBit(std::string_view s, size_t i) {
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
// Returns a pseudo-random value of length |encoding_length_in_bytes| that is
// derived from |secret|, |purpose|, |form_signature|, |field_signature| and
// |data_type|.
std::string GetPseudoRandomBits(std::string_view secret,
                                std::string_view purpose,
                                FormSignature form_signature,
                                FieldSignature field_signature,
                                std::string_view data_type,
                                int encoding_length_in_bytes) {
  // The purpose and data_type strings are expect to be small semantic
  // identifiers: "noise", "coins", "css_class", "html-name", "html_id", etc.
  int purpose_length = base::checked_cast<int>(purpose.length());
  int data_type_length = base::checked_cast<int>(data_type.length());

  // Join the descriptive information about the encoding about to be performed.
  std::string info = base::StringPrintf(
      "%d:%.*s;%08" PRIx64 ";%08" PRIx64 ";%d:%.*s", purpose_length,
      purpose_length, purpose.data(), form_signature.value(),
      static_cast<uint64_t>(field_signature.value()), data_type_length,
      data_type_length, data_type.data());

  DVLOG(1) << "Generating pseudo-random bits from " << info;

  // Generate the pseudo-random bits.
  return crypto::HkdfSha256(secret, {}, info, encoding_length_in_bytes);
}

// Returns the "random" encoding type to use for encoding.
AutofillRandomizedValue_EncodingType GetEncodingType(const std::string& seed) {
  DCHECK(!seed.empty());

  // "Randomly" select one of eligible encodings. This "random" selection is
  // persistent in that it is based directly on the persistent seed.
  const uint8_t rand_byte = static_cast<uint8_t>(seed.front());
  // Send either the EVEN_BITS or ODD_BITS.
  const AutofillRandomizedValue_EncodingType encoding_type =
      rand_byte % 2 ? AutofillRandomizedValue_EncodingType_ODD_BITS
                    : AutofillRandomizedValue_EncodingType_EVEN_BITS;

  DCHECK_NE(encoding_type,
            AutofillRandomizedValue_EncodingType_UNSPECIFIED_ENCODING_TYPE);
  return encoding_type;
}

// Returns the "random" seed to use for encoding.
std::string GetEncodingSeed(PrefService* pref_service) {
  // Get the persistent seed to use for the randomization.
  std::string s = pref_service->GetString(prefs::kAutofillUploadEncodingSeed);
  if (s.empty()) {
    s = base::UnguessableToken::Create().ToString();
    pref_service->SetString(prefs::kAutofillUploadEncodingSeed, s);
  }
  return s;
}

}  // namespace

// static
std::optional<RandomizedEncoder> RandomizedEncoder::Create(
    PrefService* pref_service) {
  // Early abort if metadata uploads are not enabled.
  if (!pref_service) {
    return std::nullopt;
  }

  // For a given `pref_service`, the seed and encoding type are constant.
  std::string seed = GetEncodingSeed(pref_service);
  const AutofillRandomizedValue_EncodingType encoding_type =
      GetEncodingType(seed);
  bool anonymous_url_collection_is_enabled = pref_service->GetBoolean(
      RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled);
  return RandomizedEncoder(std::move(seed), encoding_type,
                           anonymous_url_collection_is_enabled);
}

RandomizedEncoder::RandomizedEncoder(
    std::string seed,
    AutofillRandomizedValue_EncodingType encoding_type,
    bool anonymous_url_collection_is_enabled)
    : seed_(std::move(seed)),
      encoding_type_(encoding_type),
      anonymous_url_collection_is_enabled_(
          anonymous_url_collection_is_enabled) {
  DCHECK(AutofillRandomizedValue_EncodingType_IsValid(encoding_type_));
  DCHECK_NE(encoding_type_,
            AutofillRandomizedValue_EncodingType_UNSPECIFIED_ENCODING_TYPE);
}

RandomizedEncoder::RandomizedEncoder(RandomizedEncoder&&) = default;
RandomizedEncoder& RandomizedEncoder::operator=(RandomizedEncoder&&) = default;
RandomizedEncoder::~RandomizedEncoder() = default;

const RandomizedEncoder::EncodingInfo& RandomizedEncoder::encoding_info()
    const {
  // Lookup table AutofillRandomizedValue_EncodingType --> EncodingInfo.
  static constexpr auto kEncodingInfo =
      std::to_array<RandomizedEncoder::EncodingInfo>(
          {// One bit per byte. These all require 8 bytes to encode and have
           // 8-bit strides, starting from a different initial bit offset.
           {8, 0, 8},
           {8, 1, 8},
           {8, 2, 8},
           {8, 3, 8},
           {8, 4, 8},
           {8, 5, 8},
           {8, 6, 8},
           {8, 7, 8},
           // Four bits per byte. These require 32 bytes to encode and have
           // 2-bit strides.
           {32, 0, 2},
           {32, 1, 2},
           // All bits per byte. This require 64 bytes to encode and has a 1-bit
           // stride.
           {64, 0, 1}});

  // Assert that our array represents all enum constants except for
  // AutofillRandomizedValue_EncodingType_UNSPECIFIED_ENCODING_TYPE.
  static_assert([]() {
    constexpr int kMin = AutofillRandomizedValue_EncodingType_EncodingType_MIN;
    constexpr int kMax = AutofillRandomizedValue_EncodingType_EncodingType_MAX;
    constexpr int kUnspecified =
        AutofillRandomizedValue_EncodingType_UNSPECIFIED_ENCODING_TYPE;
    for (int i = kMin; i <= kMax; ++i) {
      if (i != kUnspecified &&
          (i < 0 || i >= static_cast<int>(kEncodingInfo.size()))) {
        return false;
      }
    }
    return true;
  }());

  return kEncodingInfo[encoding_type_];
}

std::string RandomizedEncoder::Encode(FormSignature form_signature,
                                      FieldSignature field_signature,
                                      std::string_view data_type,
                                      std::string_view data_value) const {
  size_t chunk_count = GetChunkCount(data_value, data_type);
  size_t padded_input_length_in_bytes =
      chunk_count * kEncodedChunkLengthInBytes;
  size_t padded_input_length_in_bits =
      padded_input_length_in_bytes * kBitsPerByte;

  std::string coins = GetCoins(form_signature, field_signature, data_type,
                               padded_input_length_in_bytes);
  std::string noise = GetNoise(form_signature, field_signature, data_type,
                               padded_input_length_in_bytes);

  DCHECK_EQ(coins.length() % kEncodedChunkLengthInBytes, 0u);
  DCHECK_EQ(noise.length() % kEncodedChunkLengthInBytes, 0u);
  DCHECK_EQ(coins.length(), padded_input_length_in_bytes);
  DCHECK_EQ(noise.length(), padded_input_length_in_bytes);

  // If we're encoding the bits encoding we can simply repurpose the noise
  // vector and use the coins vector merge in the selected data value bits.
  // For each bit, the encoded value is the true value if the coin-toss is TRUE
  // or the noise value if the coin-toss is FALSE. All the bits in a given byte
  // can be computed in parallel. The trailing bytes are all noise.
  if (encoding_type_ == AutofillRandomizedValue_EncodingType_ALL_BITS) {
    std::string all_bits = std::move(noise);
    const size_t value_length =
        std::min(data_value.length(), padded_input_length_in_bytes);
    for (size_t i = 0; i < value_length; ++i) {
      // Initially this byte is all noise, we're replacing the bits for which
      // the coin toss is 1 with the corresponding data_value bits, and keeping
      // the noise bits where the coin toss is 0.
      all_bits[i] = (data_value[i] & coins[i]) | (all_bits[i] & ~coins[i]);
    }
    return all_bits;
  }

  // Otherwise, pack the select the subset of bits into an output buffer.
  // This encodes every |encoding_info().bit_stride| bit starting from
  // |encoding_info().bit_offset|.
  //
  // For each bit, the encoded value is the true value if the coin-toss is TRUE
  // or the noise value if the coin-toss is FALSE. All the bits in a given byte
  // can be computed in parallel. The trailing bytes are all noise.
  const size_t output_length_in_bytes =
      encoding_info().chunk_length_in_bytes * chunk_count;
  std::string output(output_length_in_bytes, 0);
  const size_t value_length_in_bits = data_value.length() * kBitsPerByte;
  size_t dst_offset = 0;
  size_t src_offset = encoding_info().bit_offset;
  while (src_offset < padded_input_length_in_bits) {
    uint8_t output_bit = GetBit(noise, src_offset);
    if (src_offset < value_length_in_bits) {
      const uint8_t coin_bit = GetBit(coins, src_offset);
      const uint8_t data_bit = GetBit(data_value, src_offset);
      output_bit = ((coin_bit & data_bit) | (~coin_bit & output_bit));
    }
    SetBit(dst_offset, output_bit, &output);
    src_offset += encoding_info().bit_stride;
    dst_offset += 1;
  }

  DCHECK_EQ(dst_offset,
            encoding_info().chunk_length_in_bytes * chunk_count * kBitsPerByte);
  return output;
}

std::string RandomizedEncoder::EncodeForTesting(
    FormSignature form_signature,
    FieldSignature field_signature,
    std::string_view data_type,
    std::u16string_view data_value) const {
  return Encode(form_signature, field_signature, data_type,
                base::UTF16ToUTF8(data_value));
}

std::string RandomizedEncoder::GetCoins(FormSignature form_signature,
                                        FieldSignature field_signature,
                                        std::string_view data_type,
                                        int encoding_length_in_bytes) const {
  return GetPseudoRandomBits(seed_, "coins", form_signature, field_signature,
                             data_type, encoding_length_in_bytes);
}

// Get the pseudo-random string to use at the noise bit-field.
std::string RandomizedEncoder::GetNoise(FormSignature form_signature,
                                        FieldSignature field_signature,
                                        std::string_view data_type,
                                        int encoding_length_in_bytes) const {
  return GetPseudoRandomBits(seed_, "noise", form_signature, field_signature,
                             data_type, encoding_length_in_bytes);
}

int RandomizedEncoder::GetChunkCount(std::string_view data_value,
                                     std::string_view data_type) const {
  if (data_type == RandomizedEncoder::kFormUrl) {
    // ceil(data_value.length / kEncodedChunkLengthInBytes).
    int chunks = (data_value.length() + kEncodedChunkLengthInBytes - 1) /
                 kEncodedChunkLengthInBytes;
    return std::min(chunks, static_cast<int>(kMaxChunks));
  } else {
    return 1;
  }
}

}  // namespace autofill
