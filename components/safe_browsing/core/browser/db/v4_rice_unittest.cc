// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_rice.h"
#include "base/logging.h"
#include "testing/platform_test.h"

using ::google::protobuf::int32;
using ::google::protobuf::RepeatedField;

namespace safe_browsing {

class V4RiceTest : public PlatformTest {
 public:
  V4RiceTest() {}

  struct RiceDecodingTestInfo {
    uint32_t rice_parameter;
    std::vector<uint32_t> expected_values;
    std::string encoded_string;

    RiceDecodingTestInfo(const uint32_t in_rice_parameter,
                         const std::vector<uint32_t>& in_expected_values,
                         const std::string& in_encoded_string) {
      rice_parameter = in_rice_parameter;
      expected_values = in_expected_values;
      encoded_string = in_encoded_string;
    }
  };

  void VerifyRiceDecoding(const RiceDecodingTestInfo& test_info) {
    const uint32_t num_entries = test_info.expected_values.size();
    V4RiceDecoder decoder(test_info.rice_parameter, num_entries,
                          test_info.encoded_string);
    uint32_t word;
    for (const auto& expected : test_info.expected_values) {
      EXPECT_EQ(DECODE_SUCCESS, decoder.GetNextValue(&word));
      EXPECT_EQ(expected, word);
    }
    ASSERT_FALSE(decoder.HasAnotherValue());
  }
};

TEST_F(V4RiceTest, TestDecoderGetNextWordWithNoData) {
  uint32_t word;
  V4RiceDecoder decoder(5, 1, "");
  EXPECT_EQ(DECODE_RAN_OUT_OF_BITS_FAILURE, decoder.GetNextWord(&word));
}

TEST_F(V4RiceTest, TestDecoderGetNextBitsWithNoData) {
  uint32_t word;
  V4RiceDecoder decoder(5, 1, "");
  EXPECT_EQ(DECODE_RAN_OUT_OF_BITS_FAILURE, decoder.GetNextBits(1, &word));
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithNoData) {
  uint32_t word;
  V4RiceDecoder decoder(5, 1, "");
  EXPECT_EQ(DECODE_RAN_OUT_OF_BITS_FAILURE, decoder.GetNextValue(&word));
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithNoEntries) {
  uint32_t word;
  V4RiceDecoder decoder(28, 0, "\xbf\xa8");
  ASSERT_FALSE(decoder.HasAnotherValue());
  EXPECT_EQ(DECODE_NO_MORE_ENTRIES_FAILURE, decoder.GetNextValue(&word));
}

TEST_F(V4RiceTest, TestDecoderGetNextValueWithInterestingValues) {
  // These values are interesting because they match the unit test
  // values used within Google to this test this code in other
  // components, such as the SafeBrowsing service itself.

  std::vector<RiceDecodingTestInfo> test_inputs = {
      RiceDecodingTestInfo(2, {15, 9}, "\xf7\x2"),
      RiceDecodingTestInfo(
          28, {1777762129, 2093280223, 924369848},
          "\xbf\xa8\x3f\xfb\xfc\xfb\x5e\x27\xe6\xc3\x1d\xc6\x38"),
      RiceDecodingTestInfo(
          28, {62763050, 1046523781, 192522171, 1800511020, 4442775, 582142548},
          "\x54\x60\x7b\xe7\x0a\x5f\xc1\xdc\xee\x69\xde"
          "\xfe\x58\x3c\xa3\xd6\xa5\xf2\x10\x8c\x4a\x59"
          "\x56\x00"),
      RiceDecodingTestInfo(
          28,
          {26067715, 344823336, 8420095, 399843890, 95029378, 731622412,
           35811335, 1047558127, 1117722715, 78698892},
          "\x06\x86\x1b\x23\x14\xcb\x46\xf2\xaf\x07\x08\xc9\x88\x54\x1f\x41\x04"
          "\xd5\x1a\x03\xeb\xe6\x3a\x80\x13\x91\x7b\xbf\x83\xf3\xb7\x85\xf1\x29"
          "\x18\xb3\x61\x09"),
      RiceDecodingTestInfo(
          27,
          {225846818, 328287420, 166748623, 29117720, 552397365, 350353215,
           558267528, 4738273, 567093445, 28563065, 55077698, 73091685,
           339246010, 98242620, 38060941, 63917830, 206319759, 137700744},
          "\x89\x98\xd8\x75\xbc\x44\x91\xeb\x39\x0c\x3e\x30\x9a\x78\xf3\x6a\xd4"
          "\xd9\xb1\x9f\xfb\x70\x3e\x44\x3e\xa3\x08\x67\x42\xc2\x2b\x46\x69\x8e"
          "\x3c\xeb\xd9\x10\x5a\x43\x9a\x32\xa5\x2d\x4e\x77\x0f\x87\x78\x20\xb6"
          "\xab\x71\x98\x48\x0c\x9e\x9e\xd7\x23\x0c\x13\x43\x2c\xa9\x01"),
      RiceDecodingTestInfo(
          28,
          {339784008, 263128563, 63871877, 69723256, 826001074, 797300228,
           671166008, 207712688},
          std::string("\x21\xc5\x02\x91\xf9\x82\xd7\x57\xb8\xe9\x3c\xf0\xc8\x4f"
                      "\xe8\x64\x8d\x77\x62\x04\xd6\x85\x3f\x1c\x97\x00\x04\x1b"
                      "\x17\xc6",
                      30)),
      RiceDecodingTestInfo(
          28,
          {471820069, 196333855, 855579133, 122737976, 203433838, 85354544,
           1307949392, 165938578, 195134475, 553930435, 49231136},
          "\x95\x9c\x7d\xb0\x8f\xe8\xd9\xbd\xfe\x8c\x7f\x81\x53\x0d\x75\xdc\x4e"
          "\x40\x18\x0c\x9a\x45\x3d\xa8\xdc\xfa\x26\x59\x40\x9e\x16\x08\x43\x77"
          "\xc3\x4e\x04\x01\xa4\xe6\x5d\x00"),
      RiceDecodingTestInfo(
          27,
          {87336845, 129291033, 30906211, 433549264, 30899891, 53207875,
           11959529, 354827862, 82919275, 489637251, 53561020, 336722992,
           408117728, 204506246, 188216092, 9047110, 479817359, 230317256},
          "\x1a\x4f\x69\x2a\x63\x9a\xf6\xc6\x2e\xaf\x73\xd0\x6f\xd7\x31\xeb\x77"
          "\x1d\x43\xe3\x2b\x93\xce\x67\x8b\x59\xf9\x98\xd4\xda\x4f\x3c\x6f\xb0"
          "\xe8\xa5\x78\x8d\x62\x36\x18\xfe\x08\x1e\x78\xd8\x14\x32\x24\x84\x61"
          "\x1c\xf3\x37\x63\xc4\xa0\x88\x7b\x74\xcb\x64\xc8\x5c\xba\x05"),
      RiceDecodingTestInfo(
          28,
          {297968956, 19709657, 259702329, 76998112, 1023176123, 29296013,
           1602741145, 393745181, 177326295, 55225536, 75194472},
          "\xf1\x94\x0a\x87\x6c\x5f\x96\x90\xe3\xab\xf7\xc0\xcb\x2d\xe9\x76\xdb"
          "\xf8\x59\x63\xc1\x6f\x7c\x99\xe3\x87\x5f\xc7\x04\xde\xb9\x46\x8e\x54"
          "\xc0\xac\x4a\x03\x0d\x6c\x8f\x00"),
      RiceDecodingTestInfo(
          28,
          {532220688, 780594691, 436816483, 163436269, 573044456, 1069604,
           39629436, 211410997, 227714491, 381562898, 75610008, 196754597,
           40310339, 15204118, 99010842},
          "\x41\x2c\xe4\xfe\x06\xdc\x0d\xbd\x31\xa5\x04\xd5\x6e\xdd\x9b\x43\xb7"
          "\x3f\x11\x24\x52\x10\x80\x4f\x96\x4b\xd4\x80\x67\xb2\xdd\x52\xc9\x4e"
          "\x02\xc6\xd7\x60\xde\x06\x92\x52\x1e\xdd\x35\x64\x71\x26\x2c\xfe\xcf"
          "\x81\x46\xb2\x79\x01"),
      RiceDecodingTestInfo(
          28,
          {219354713, 389598618, 750263679, 554684211, 87381124, 4523497,
           287633354, 801308671, 424169435, 372520475, 277287849},
          "\xb2\x2c\x26\x3a\xcd\x66\x9c\xdb\x5f\x07\x2e\x6f\xe6\xf9\x21\x10\x52"
          "\xd5\x94\xf4\x82\x22\x48\xf9\x9d\x24\xf6\xff\x2f\xfc\x6d\x3f\x21\x65"
          "\x1b\x36\x34\x56\xea\xc4\x21\x00"),
  };

  for (size_t i = 0; i < test_inputs.size(); i++) {
    DVLOG(1) << "Running test case: " << i;
    VerifyRiceDecoding(test_inputs[i]);
  }
}

TEST_F(V4RiceTest, TestDecoderIntegersWithNoData) {
  RepeatedField<int32> out;
  EXPECT_EQ(ENCODED_DATA_UNEXPECTED_EMPTY_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, 5, 1, "", &out));
}

TEST_F(V4RiceTest, TestDecoderIntegersWithNegativeNumEntries) {
  RepeatedField<int32> out;
  EXPECT_EQ(NUM_ENTRIES_NEGATIVE_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, 5, -1, "", &out));
}

TEST_F(V4RiceTest, TestDecoderIntegersWithNonPositiveRiceParameter) {
  RepeatedField<int32> out;
  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, 0, 1, "a", &out));

  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodeIntegers(3, -1, 1, "a", &out));
}

TEST_F(V4RiceTest, TestDecoderIntegersWithOverflowValues) {
  RepeatedField<int32> out;
  EXPECT_EQ(DECODED_INTEGER_OVERFLOW_FAILURE,
            V4RiceDecoder::DecodeIntegers(
                5, 28, 3,
                "\xbf\xa8\x3f\xfb\xfc\xfb\x5e\x27\xe6\xc3\x1d\xc6\x38", &out));
}

TEST_F(V4RiceTest, TestDecoderIntegersWithOneValue) {
  RepeatedField<int32> out;
  EXPECT_EQ(DECODE_SUCCESS, V4RiceDecoder::DecodeIntegers(3, 2, 0, "", &out));
  EXPECT_EQ(1, out.size());
  EXPECT_EQ(3, out.Get(0));
}

TEST_F(V4RiceTest, TestDecoderIntegersWithMultipleValues) {
  RepeatedField<int32> out;
  EXPECT_EQ(DECODE_SUCCESS,
            V4RiceDecoder::DecodeIntegers(5, 2, 2, "\xf7\x2", &out));
  EXPECT_EQ(3, out.size());
  EXPECT_EQ(5, out.Get(0));
  EXPECT_EQ(20, out.Get(1));
  EXPECT_EQ(29, out.Get(2));
}

TEST_F(V4RiceTest, TestDecoderPrefixesWithNoData) {
  std::vector<uint32_t> out;
  EXPECT_EQ(ENCODED_DATA_UNEXPECTED_EMPTY_FAILURE,
            V4RiceDecoder::DecodePrefixes(3, 5, 1, "", &out));
}

TEST_F(V4RiceTest, TestDecoderPrefixesWithNegativeNumEntries) {
  std::vector<uint32_t> out;
  EXPECT_EQ(NUM_ENTRIES_NEGATIVE_FAILURE,
            V4RiceDecoder::DecodePrefixes(3, 5, -1, "", &out));
}

TEST_F(V4RiceTest, TestDecoderPrefixesWithNonPositiveRiceParameter) {
  std::vector<uint32_t> out;
  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodePrefixes(3, 0, 1, "a", &out));

  EXPECT_EQ(RICE_PARAMETER_NON_POSITIVE_FAILURE,
            V4RiceDecoder::DecodePrefixes(3, -1, 1, "a", &out));
}

TEST_F(V4RiceTest, TestDecoderPrefixesWithOneValue) {
  std::vector<uint32_t> out;
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(DECODE_SUCCESS,
            V4RiceDecoder::DecodePrefixes(0x69F67F51u, 2, 0, "", &out));
  EXPECT_EQ(1u, out.size());
  EXPECT_EQ(0x69F67F51u, out[0]);
}

TEST_F(V4RiceTest, TestDecoderPrefixesWithMultipleValues) {
  std::vector<uint32_t> out;
  EXPECT_EQ(DECODE_SUCCESS,
            V4RiceDecoder::DecodePrefixes(
                5, 28, 3, "\xbf\xa8\x3f\xfb\xf\xf\x5e\x27\xe6\xc3\x1d\xc6\x38",
                &out));
  std::vector<uint32_t> expected = {5, 0xad934c0cu, 0x6ff67f56u, 0x81316fceu};
  EXPECT_EQ(expected.size(), out.size());
  for (unsigned i = 0; i < expected.size(); i++) {
    EXPECT_EQ(expected[i], out[i]);
  }
}

TEST_F(V4RiceTest, TestDecoderPrefixesWithOverflowValues) {
  std::vector<uint32_t> out;
  EXPECT_EQ(DECODED_INTEGER_OVERFLOW_FAILURE,
            V4RiceDecoder::DecodePrefixes(
                5, 28, 3,
                "\xbf\xa8\x3f\xfb\xfc\xfb\x5e\x27\xe6\xc3\x1d\xc6\x38", &out));
}

}  // namespace safe_browsing
