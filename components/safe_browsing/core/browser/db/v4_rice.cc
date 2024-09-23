// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/check_op.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/browser/db/v4_rice.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#elif BUILDFLAG(IS_POSIX)
#include <arpa/inet.h>
#endif

using ::google::protobuf::int32;
using ::google::protobuf::int64;
using ::google::protobuf::RepeatedField;

#if !defined(ARCH_CPU_LITTLE_ENDIAN) || (ARCH_CPU_LITTLE_ENDIAN != 1)
#error The code below assumes little-endianness.
#endif

namespace safe_browsing {

namespace {

const int kBitsPerByte = 8;
const unsigned int kMaxBitIndex = kBitsPerByte * sizeof(uint32_t);

}  // namespace

// static
V4DecodeResult V4RiceDecoder::ValidateInput(const int32 rice_parameter,
                                            const int32 num_entries,
                                            const std::string& encoded_data) {
  if (num_entries < 0) {
    return NUM_ENTRIES_NEGATIVE_FAILURE;
  }

  if (num_entries == 0) {
    return DECODE_SUCCESS;
  }

  if (rice_parameter <= 0) {
    return RICE_PARAMETER_NON_POSITIVE_FAILURE;
  }

  if (encoded_data.empty()) {
    return ENCODED_DATA_UNEXPECTED_EMPTY_FAILURE;
  }

  return DECODE_SUCCESS;
}

// static
V4DecodeResult V4RiceDecoder::DecodeIntegers(const int64 first_value,
                                             const int32 rice_parameter,
                                             const int32 num_entries,
                                             const std::string& encoded_data,
                                             RepeatedField<int32>* out) {
  DCHECK(out);

  V4DecodeResult result =
      ValidateInput(rice_parameter, num_entries, encoded_data);
  if (result != DECODE_SUCCESS) {
    return result;
  }

  out->Reserve(num_entries + 1);
  base::CheckedNumeric<int32> last_value(first_value);
  out->Add(last_value.ValueOrDie());
  if (num_entries == 0) {
    return DECODE_SUCCESS;
  }

  V4RiceDecoder decoder(rice_parameter, num_entries, encoded_data);
  while (decoder.HasAnotherValue()) {
    uint32_t offset;
    result = decoder.GetNextValue(&offset);
    if (result != DECODE_SUCCESS) {
      return result;
    }

    last_value += offset;
    if (!last_value.IsValid()) {
      return DECODED_INTEGER_OVERFLOW_FAILURE;
    }

    out->Add(last_value.ValueOrDie());
  }

  return DECODE_SUCCESS;
}

// static
V4DecodeResult V4RiceDecoder::DecodePrefixes(const int64 first_value,
                                             const int32 rice_parameter,
                                             const int32 num_entries,
                                             const std::string& encoded_data,
                                             std::vector<uint32_t>* out) {
  DCHECK(out);

  V4DecodeResult result =
      ValidateInput(rice_parameter, num_entries, encoded_data);
  if (result != DECODE_SUCCESS) {
    return result;
  }
  out->reserve((num_entries + 1));

  base::CheckedNumeric<uint32_t> last_value(first_value);
  out->push_back(htonl(last_value.ValueOrDie()));

  if (num_entries > 0) {
    V4RiceDecoder decoder(rice_parameter, num_entries, encoded_data);
    while (decoder.HasAnotherValue()) {
      uint32_t offset;
      result = decoder.GetNextValue(&offset);
      if (result != DECODE_SUCCESS) {
        return result;
      }

      last_value += offset;
      if (!last_value.IsValid()) {
        return DECODED_INTEGER_OVERFLOW_FAILURE;
      }

      // This flipping is done so that the decoded uint32 is interpreted
      // correcly as a string of 4 bytes.
      out->push_back(htonl(last_value.ValueOrDie()));
    }
  }

  // Flipping the bytes, as done above, destroys the sort order. Sort the
  // values back.
  std::sort(out->begin(), out->end());

  // This flipping is done so that when the vector is interpreted as a string,
  // the bytes are in the correct order.
  for (size_t i = 0; i < out->size(); i++) {
    (*out)[i] = ntohl((*out)[i]);
  }

  return DECODE_SUCCESS;
}

V4RiceDecoder::V4RiceDecoder(const int rice_parameter,
                             const int num_entries,
                             const std::string& encoded_data)
    : rice_parameter_(rice_parameter),
      num_entries_(num_entries),
      data_(encoded_data),
      current_word_(0) {
  DCHECK_LE(0, num_entries_);
  DCHECK_LE(2u, rice_parameter_);
  DCHECK_GE(28u, rice_parameter_);

  data_byte_index_ = 0;
  current_word_bit_index_ = kMaxBitIndex;
}

V4RiceDecoder::~V4RiceDecoder() {}

bool V4RiceDecoder::HasAnotherValue() const {
  return num_entries_ > 0;
}

V4DecodeResult V4RiceDecoder::GetNextValue(uint32_t* value) {
  if (!HasAnotherValue()) {
    return DECODE_NO_MORE_ENTRIES_FAILURE;
  }

  V4DecodeResult result;
  uint32_t q = 0;
  uint32_t bit;
  do {
    result = GetNextBits(1, &bit);
    if (result != DECODE_SUCCESS) {
      return result;
    }
    q += bit;
  } while (bit);
  uint32_t r = 0;
  result = GetNextBits(rice_parameter_, &r);
  if (result != DECODE_SUCCESS) {
    return result;
  }

  *value = (q << rice_parameter_) + r;
  num_entries_--;
  return DECODE_SUCCESS;
}

V4DecodeResult V4RiceDecoder::GetNextWord(uint32_t* word) {
  if (data_byte_index_ >= data_.size()) {
    return DECODE_RAN_OUT_OF_BITS_FAILURE;
  }

  const size_t mask = 0xFF;
  *word = (data_[data_byte_index_] & mask);
  data_byte_index_++;
  current_word_bit_index_ = 0;

  if (data_byte_index_ < data_.size()) {
    *word |= ((data_[data_byte_index_] & mask) << 8);
    data_byte_index_++;

    if (data_byte_index_ < data_.size()) {
      *word |= ((data_[data_byte_index_] & mask) << 16);
      data_byte_index_++;

      if (data_byte_index_ < data_.size()) {
        *word |= ((data_[data_byte_index_] & mask) << 24);
        data_byte_index_++;
      }
    }
  }

  return DECODE_SUCCESS;
}

V4DecodeResult V4RiceDecoder::GetNextBits(unsigned int num_requested_bits,
                                          uint32_t* x) {
  if (num_requested_bits > kMaxBitIndex) {
    return DECODE_REQUESTED_TOO_MANY_BITS_FAILURE;
  }

  if (current_word_bit_index_ == kMaxBitIndex) {
    V4DecodeResult result = GetNextWord(&current_word_);
    if (result != DECODE_SUCCESS) {
      return result;
    }
  }

  unsigned int num_bits_left_in_current_word =
      kMaxBitIndex - current_word_bit_index_;
  if (num_bits_left_in_current_word >= num_requested_bits) {
    // All the bits that we need are in |current_word_|.
    *x = GetBitsFromCurrentWord(num_requested_bits);
  } else {
    // |current_word_| contains fewer bits than we need so read the remaining
    // bits from |current_word_| into |lower|, and then call GetNextBits on the
    // remaining number of bits, which will read in a new word into
    // |current_word_|.
    uint32_t lower = GetBitsFromCurrentWord(num_bits_left_in_current_word);

    unsigned int num_bits_from_next_word =
        num_requested_bits - num_bits_left_in_current_word;
    uint32_t upper;
    V4DecodeResult result = GetNextBits(num_bits_from_next_word, &upper);
    if (result != DECODE_SUCCESS) {
      return result;
    }
    *x = (upper << num_bits_left_in_current_word) | lower;
  }
  return DECODE_SUCCESS;
}

uint32_t V4RiceDecoder::GetBitsFromCurrentWord(
    unsigned int num_requested_bits) {
  uint32_t mask = 0xFFFFFFFF >> (kMaxBitIndex - num_requested_bits);
  uint32_t x = current_word_ & mask;
  current_word_ = current_word_ >> num_requested_bits;
  current_word_bit_index_ += num_requested_bits;
  return x;
}

std::string V4RiceDecoder::DebugString() const {
  // Calculates the total number of bits that we have read from the buffer,
  // excluding those that have been read into current_word_ but not yet
  // consumed byt GetNextBits().
  unsigned bits_read = (data_byte_index_ - sizeof(uint32_t)) * kBitsPerByte +
                       current_word_bit_index_;
  return base::StringPrintf(
      "bits_read: %x; current_word_: %x; data_byte_index_; %x, "
      "current_word_bit_index_: %x; rice_parameter_: %x",
      bits_read, current_word_, data_byte_index_, current_word_bit_index_,
      rice_parameter_);
}

std::ostream& operator<<(std::ostream& os, const V4RiceDecoder& rice_decoder) {
  os << rice_decoder.DebugString();
  return os;
}

}  // namespace safe_browsing
