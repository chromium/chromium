// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/name_value_pairs_parser.h"

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::system {

// A parameterized test class running tests for all the formats that are
// compatible with VPD dumps.
class VpdDumpNameValuePairsParserTest
    : public testing::TestWithParam<NameValuePairsFormat> {
 protected:
  NameValuePairsParser::NameValueMap map;
};

TEST_P(VpdDumpNameValuePairsParserTest, TestParseNameValuePairs) {
  const NameValuePairsFormat format = GetParam();
  NameValuePairsParser parser(&map);

  const std::string contents1 = R"(
"initial_locale"="ja"
"keyboard_layout"="mozc-jp"
"empty"=""
)";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents1, format));
  EXPECT_EQ(3U, map.size());
  EXPECT_EQ("ja", map["initial_locale"]);
  EXPECT_EQ("mozc-jp", map["keyboard_layout"]);
  EXPECT_EQ("", map["empty"]);

  map.clear();
  const std::string contents2 = R"(
"quoted"=""quote""
"serial_number"="BADBADBAD""
"model_name"="15" Chromebook"
)";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents2, format));
  EXPECT_EQ(3U, map.size());
  EXPECT_EQ("\"quote\"", map["quoted"]);
  EXPECT_EQ("BADBADBAD\"", map["serial_number"]);
  EXPECT_EQ("15\" Chromebook", map["model_name"]);

  map.clear();
  const std::string contents3 = R"(
"a"="
"a"=
"a=b"="c"
""="value"
)";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents3, format));
  EXPECT_EQ(0U, map.size());
}

INSTANTIATE_TEST_SUITE_P(NameValuePairs,
                         VpdDumpNameValuePairsParserTest,
                         testing::Values(NameValuePairsFormat::kVpdDump,
                                         NameValuePairsFormat::kMachineInfo));

TEST(NameValuePairsParser, TestParseNameValuePairsInVpdDumpFormat) {
  constexpr NameValuePairsFormat format = NameValuePairsFormat::kVpdDump;
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);

  // Names must be quoted in VPD dump format. Unquoted names are ignored.
  const std::string contents1 = R"(
"name1"="value1"
name2="value2"
"name3"="value3"
name4="value4"
)";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents1, format));
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ("value1", map["name1"]);
  EXPECT_EQ("value3", map["name3"]);
}

TEST(NameValuePairsParser, TestParseErrorInVpdDumpFormat) {
  constexpr NameValuePairsFormat format = NameValuePairsFormat::kVpdDump;
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);

  // Names must be quoted in VPD dump format. Unquoted names are ignored.
  const std::string contents1 = R"(
"name1"="value1"
# RW_VPD execute error.
)";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents1, format));
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ("value1", map["name1"]);
}

TEST(NameValuePairsParser, TestParseNameValuePairsInMachineInfoFormat) {
  constexpr NameValuePairsFormat format = NameValuePairsFormat::kMachineInfo;
  NameValuePairsParser::NameValueMap map;
  NameValuePairsParser parser(&map);

  // Names do not have to be quoted in machine info format.
  const std::string contents1 = R"(
"name1"="value1"
name2="value2"
"name3"="value3"
name4="value4"
)";
  EXPECT_TRUE(parser.ParseNameValuePairs(contents1, format));
  EXPECT_EQ(4U, map.size());
  EXPECT_EQ("value1", map["name1"]);
  EXPECT_EQ("value2", map["name2"]);
  EXPECT_EQ("value3", map["name3"]);
  EXPECT_EQ("value4", map["name4"]);

  map.clear();
  const std::string contents2 = R"(
="value"
)";
  EXPECT_FALSE(parser.ParseNameValuePairs(contents2, format));
  EXPECT_EQ(0U, map.size());
}

}  // namespace ash::system
