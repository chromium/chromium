// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/randomized_encoder.h"

#include <algorithm>
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
constexpr size_t kEncodedChunkLengthInBytes = 64;
constexpr size_t kMaxChunks = 8;

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

const char RandomizedEncoder::FORM_ID[] = "form-id";
const char RandomizedEncoder::FORM_NAME[] = "form-name";
const char RandomizedEncoder::FORM_ACTION[] = "form-action";
const char RandomizedEncoder::FORM_URL[] = "form-url";
const char RandomizedEncoder::FORM_CSS_CLASS[] = "form-css-class";
const char RandomizedEncoder::FORM_BUTTON_TITLES[] = "button-titles";

const char RandomizedEncoder::FIELD_ID[] = "field-id";
const char RandomizedEncoder::FIELD_NAME[] = "field-name";
const char RandomizedEncoder::FIELD_CONTROL_TYPE[] = "field-control-type";
const char RandomizedEncoder::FIELD_LABEL[] = "field-label";
const char RandomizedEncoder::FIELD_ARIA_LABEL[] = "field-aria-label";
const char RandomizedEncoder::FIELD_ARIA_DESCRIPTION[] =
    "field-aria-description";
const char RandomizedEncoder::FIELD_CSS_CLASS[] = "field-css-classes";
const char RandomizedEncoder::FIELD_PLACEHOLDER[] = "field-placeholder";
const char RandomizedEncoder::FIELD_INITIAL_VALUE_HASH[] =
    "field-initial-hash-value";
const char RandomizedEncoder::FIELD_AUTOCOMPLETE[] = "field-autocomplete";

// Copy of components/unified_consent/pref_names.cc
// We could not use the constant from components/unified_constants because of a
// circular dependency.
// TODO(crbug.com/40570965): resolve circular dependency and remove
// hardcoded constant
const char RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled[] =
    "url_keyed_anonymized_data_collection.enabled";

// static
std::unique_ptr<RandomizedEncoder> RandomizedEncoder::Create(
    PrefService* pref_service) {
  // Early abort if metadata uploads are not enabled.
  if (!pref_service) {
    return nullptr;
  }

  // Return the randomized encoder. Note that for a given client, the seed and
  // encoding type are constant via prefs/config.
  const auto seed = GetEncodingSeed(pref_service);
  const auto encoding_type = GetEncodingType(seed);
  bool anonymous_url_collection_is_enabled = pref_service->GetBoolean(
      RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled);
  return std::make_unique<RandomizedEncoder>(
      std::move(seed), encoding_type, anonymous_url_collection_is_enabled);
}

RandomizedEncoder::RandomizedEncoder(
    std::string seed,
    AutofillRandomizedValue_EncodingType encoding_type,
    bool anonymous_url_collection_is_enabled)
    : seed_(std::move(seed)),
      encoding_info_(GetEncodingInfo(encoding_type)),
      anonymous_url_collection_is_enabled_(
          anonymous_url_collection_is_enabled) {
  DCHECK(encoding_info_ != nullptr);
}

std::string RandomizedEncoder::Encode(FormSignature form_signature,
                                      FieldSignature field_signature,
                                      std::string_view data_type,
                                      std::string_view data_value) const {
  if (!encoding_info_) {
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }

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
  if (encoding_info_->encoding_type ==
      AutofillRandomizedValue_EncodingType_ALL_BITS) {
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
  // This encodes every |encoding_info_->bit_stride| bit starting from
  // |encoding_info_->bit_offset|.
  //
  // For each bit, the encoded value is the true value if the coin-toss is TRUE
  // or the noise value if the coin-toss is FALSE. All the bits in a given byte
  // can be computed in parallel. The trailing bytes are all noise.
  const size_t output_length_in_bytes =
      encoding_info_->chunk_length_in_bytes * chunk_count;
  std::string output(output_length_in_bytes, 0);
  const size_t value_length_in_bits = data_value.length() * kBitsPerByte;
  size_t dst_offset = 0;
  size_t src_offset = encoding_info_->bit_offset;
  while (src_offset < padded_input_length_in_bits) {
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

  DCHECK_EQ(dst_offset,
            encoding_info_->chunk_length_in_bytes * chunk_count * kBitsPerByte);
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
  if (data_type == RandomizedEncoder::FORM_URL) {
    // ceil(data_value.length / kEncodedChunkLengthInBytes).
    int chunks = (data_value.length() + kEncodedChunkLengthInBytes - 1) /
                 kEncodedChunkLengthInBytes;
    return std::min(chunks, static_cast<int>(kMaxChunks));
  } else {
    return 1;
  }
}

}  // namespace autofill
