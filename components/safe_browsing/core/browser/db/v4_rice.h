// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Rice-Golomb decoder for blocklist updates.
// Details at: https://en.wikipedia.org/wiki/Golomb_coding

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_RICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_RICE_H_

#include <ostream>
#include <string>
#include <vector>
#include "base/gtest_prod_util.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace safe_browsing {

// Enumerate different failure events while decoding the Rice-encoded string
// sent by the server for histogramming purposes. DO NOT CHANGE THE ORDERING OF
// THESE VALUES.
enum V4DecodeResult {
  // No error.
  DECODE_SUCCESS = 0,

  // Exceeded the number of entries to expect.
  DECODE_NO_MORE_ENTRIES_FAILURE = 1,

  // Requested to decode >32 bits.
  DECODE_REQUESTED_TOO_MANY_BITS_FAILURE = 2,

  // All bits had already been read and interpreted in the encoded string.
  DECODE_RAN_OUT_OF_BITS_FAILURE = 3,

  // The num_entries argument to DecodePrefixes or DecodeIntegers was negative.
  NUM_ENTRIES_NEGATIVE_FAILURE = 4,

  // Rice-encoding parameter was non-positive when the number of encoded entries
  // was > 0.
  RICE_PARAMETER_NON_POSITIVE_FAILURE = 5,

  // |encoded_data| was empty when the number of encoded entries was > 0.
  ENCODED_DATA_UNEXPECTED_EMPTY_FAILURE = 6,

  // decoded value had an integer overflow, which is unexpected.
  DECODED_INTEGER_OVERFLOW_FAILURE = 7,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  DECODE_RESULT_MAX
};

class V4RiceDecoder {
 public:
  // Decodes the Rice-encoded string in |encoded_data| as a list of integers
  // and stores them in |out|. |rice_parameter| is the exponent of 2 for
  // calculating 'M', |first_value| is the first value in the output sequence,
  // |num_entries| is the number of subsequent encoded entries. Each decoded
  // value is a positive offset from the previous value.
  // So, for instance, if the unencoded sequence is: [3, 7, 25], then
  // |first_value| = 3, |num_entries| = 2 and decoding the |encoded_data| will
  // produce the offsets: [4, 18].
  static V4DecodeResult DecodeIntegers(
      const ::google::protobuf::int64 first_value,
      const ::google::protobuf::int32 rice_parameter,
      const ::google::protobuf::int32 num_entries,
      const std::string& encoded_data,
      ::google::protobuf::RepeatedField<::google::protobuf::int32>* out);

  // Decodes the Rice-encoded string in |encoded_data| as a string of 4-byte
  // hash prefixes and stores them in |out|. The rest of the arguments are the
  // same as for |DecodeIntegers|.
  // Important: |out| is only meant to be used as a concatenated list of sorted
  // 4-byte hash prefixes, not as a vector of uint32_t values.
  // This method does the following:
  // 1. Rice-decode the |encoded_data| as a list of uint32_t values.
  // 2. Flip the endianness (on little-endian machines) of each of these
  //    values. This is done because when a hash prefix is represented as a
  //    uint32_t, the bytes get reordered. This generates the hash prefix that
  //    the server would have sent in the absence of Rice-encoding.
  // 3. Sort the resulting list of uint32_t values.
  // 4. Flip the endianness once again since the uint32_t are expected to be
  //    consumed as a concatenated list of 4-byte hash prefixes, when merging
  //    the
  //    update with the existing state.
  static V4DecodeResult DecodePrefixes(
      const ::google::protobuf::int64 first_value,
      const ::google::protobuf::int32 rice_parameter,
      const ::google::protobuf::int32 num_entries,
      const std::string& encoded_data,
      std::vector<uint32_t>* out);

  virtual ~V4RiceDecoder();

  std::string DebugString() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(V4RiceTest, TestDecoderGetNextWordWithNoData);
  FRIEND_TEST_ALL_PREFIXES(V4RiceTest, TestDecoderGetNextBitsWithNoData);
  FRIEND_TEST_ALL_PREFIXES(V4RiceTest, TestDecoderGetNextValueWithNoData);
  FRIEND_TEST_ALL_PREFIXES(V4RiceTest, TestDecoderGetNextValueWithNoEntries);
  friend class V4RiceTest;

  // Validate some of the parameters passed to the decode methods.
  static V4DecodeResult ValidateInput(
      const ::google::protobuf::int32 rice_parameter,
      const ::google::protobuf::int32 num_entries,
      const std::string& encoded_data);

  // The |rice_parameter| is the exponent of 2 for calculating 'M',
  // |num_entries| is the number of encoded entries in the |encoded_data| and
  // |encoded_data| is the Rice-encoded string to decode.
  V4RiceDecoder(const ::google::protobuf::int32 rice_parameter,
                const ::google::protobuf::int32 num_entries,
                const std::string& encoded_data);

  // Returns true until |num_entries| entries have been decoded.
  bool HasAnotherValue() const;

  // Populates |value| with the next 32-bit unsigned integer decoded from
  // |encoded_data|.
  V4DecodeResult GetNextValue(uint32_t* value);

  // Reads in up to 32 bits from |encoded_data| into |word|, from which
  // subsequent GetNextBits() calls read bits.
  V4DecodeResult GetNextWord(uint32_t* word);

  // Reads |num_requested_bits| into |x| from |current_word_| and advances it
  // if needed by calling GetNextWord().
  V4DecodeResult GetNextBits(unsigned int num_requested_bits, uint32_t* x);

  // Reads |num_requested_bits| from |current_word_|.
  uint32_t GetBitsFromCurrentWord(unsigned int num_requested_bits);

  // The Rice parameter, which is the exponent of two for calculating 'M'. 'M'
  // is used as the base to calculate the quotient and remainder in the
  // algorithm.
  const unsigned int rice_parameter_;

  // The number of entries encoded in the data stream.
  ::google::protobuf::int32 num_entries_;

  // The Rice-encoded string.
  const std::string data_;

  // Represents how many total bytes have we read from |data_| into
  // |current_word_|.
  unsigned int data_byte_index_;

  // Represents the number of bits that we have read from |current_word_|. When
  // this becomes 32, which is the size of |current_word_|, a new
  // |current_word_| needs to be read from |data_|.
  unsigned int current_word_bit_index_;

  // The 32-bit value read from |data_|. All bit reading operations operate on
  // |current_word_|.
  uint32_t current_word_;
};

std::ostream& operator<<(std::ostream& os, const V4RiceDecoder& rice_decoder);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_DB_V4_RICE_H_
