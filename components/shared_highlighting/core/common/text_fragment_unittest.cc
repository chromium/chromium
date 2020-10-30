// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragment.h"

#include "base/values.h"
#include "components/shared_highlighting/core/common/text_fragments_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

base::Value TextFragmentToValue(const std::string& fragment) {
  base::Optional<TextFragment> opt_frag =
      TextFragment::FromEscapedString(fragment);
  return opt_frag ? opt_frag->ToValue() : base::Value(base::Value::Type::NONE);
}

TEST(TextFragmentTest, FragmentToValueFromEncodedString) {
  // Success cases
  std::string fragment = "start";
  base::Value result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "start,end";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "prefix-,start";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kFragmentPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "start,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_EQ("suffix", result.FindKey(kFragmentSuffixKey)->GetString());

  fragment = "prefix-,start,end";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kFragmentPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  fragment = "start,end,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kFragmentSuffixKey)->GetString());

  fragment = "prefix-,start,end,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ("prefix", result.FindKey(kFragmentPrefixKey)->GetString());
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_EQ("end", result.FindKey(kFragmentTextEndKey)->GetString());
  EXPECT_EQ("suffix", result.FindKey(kFragmentSuffixKey)->GetString());

  // Trailing comma doesn't break otherwise valid fragment
  fragment = "start,";
  result = TextFragmentToValue(fragment);
  EXPECT_FALSE(result.FindKey(kFragmentPrefixKey));
  EXPECT_EQ("start", result.FindKey(kFragmentTextStartKey)->GetString());
  EXPECT_FALSE(result.FindKey(kFragmentTextEndKey));
  EXPECT_FALSE(result.FindKey(kFragmentSuffixKey));

  // Failure Cases
  fragment = "";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "some,really-,malformed,-thing,with,too,many,commas";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "start,prefix-,-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-,-suffix,start";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "prefix-";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());

  fragment = "-suffix";
  result = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result.type());
}

TEST(TextFragmentTest, FragmentToEscapedStringEmpty) {
  EXPECT_EQ("", TextFragment("").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringEmptyTextStart) {
  EXPECT_EQ("", TextFragment("", "a", "b", "c").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringOnlyTextStart) {
  EXPECT_EQ("text=only%20start", TextFragment("only start").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringWithTextEnd) {
  EXPECT_EQ("text=only%20start,and%20end",
            TextFragment("only start", "and end", "", "").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringWithPrefix) {
  EXPECT_EQ("text=and%20prefix-,only%20start",
            TextFragment("only start", "", "and prefix", "").ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringWithPrefixAndSuffix) {
  EXPECT_EQ("text=and%20prefix-,only%20start,-and%20suffix",
            TextFragment("only start", "", "and prefix", "and suffix")
                .ToEscapedString());
}

TEST(TextFragmentTest, FragmentToEscapedStringAllWithSpecialCharacters) {
  TextFragment test_fragment("text, Start-&", "end of, & Text-", "pre-fix&, !",
                             "suff,i,x-+&");
  EXPECT_EQ(
      "text=pre%2Dfix%26%2C%20!-,"
      "text%2C%20Start%2D%26"
      ",end%20of%2C%20%26%20Text%2D"
      ",-suff%2Ci%2Cx%2D%2B%26",
      test_fragment.ToEscapedString());
}

}  // namespace
}  // namespace shared_highlighting
