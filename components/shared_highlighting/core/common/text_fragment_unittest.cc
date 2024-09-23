// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragment.h"

#include "base/values.h"
#include "components/shared_highlighting/core/common/fragment_directives_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

base::Value TextFragmentToValue(const std::string& fragment) {
  std::optional<TextFragment> opt_frag =
      TextFragment::FromEscapedString(fragment);
  return opt_frag ? opt_frag->ToValue() : base::Value(base::Value::Type::NONE);
}

TEST(TextFragmentTest, FragmentToValueFromEncodedString) {
  // Success cases
  std::string fragment = "start";
  base::Value::Dict result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_FALSE(result.contains(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_FALSE(result.contains(kFragmentTextEndKey));
  EXPECT_FALSE(result.contains(kFragmentSuffixKey));

  fragment = "start,end";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_FALSE(result.contains(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_EQ("end", *result.FindString(kFragmentTextEndKey));
  EXPECT_FALSE(result.contains(kFragmentSuffixKey));

  fragment = "prefix-,start";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_EQ("prefix", *result.FindString(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_FALSE(result.contains(kFragmentTextEndKey));
  EXPECT_FALSE(result.contains(kFragmentSuffixKey));

  fragment = "start,-suffix";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_FALSE(result.contains(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_FALSE(result.contains(kFragmentTextEndKey));
  EXPECT_EQ("suffix", *result.FindString(kFragmentSuffixKey));

  fragment = "prefix-,start,end";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_EQ("prefix", *result.FindString(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_EQ("end", *result.FindString(kFragmentTextEndKey));
  EXPECT_FALSE(result.contains(kFragmentSuffixKey));

  fragment = "start,end,-suffix";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_FALSE(result.contains(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_EQ("end", *result.FindString(kFragmentTextEndKey));
  EXPECT_EQ("suffix", *result.FindString(kFragmentSuffixKey));

  fragment = "prefix-,start,end,-suffix";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_EQ("prefix", *result.FindString(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_EQ("end", *result.FindString(kFragmentTextEndKey));
  EXPECT_EQ("suffix", *result.FindString(kFragmentSuffixKey));

  // Trailing comma doesn't break otherwise valid fragment
  fragment = "start,";
  result = TextFragmentToValue(fragment).TakeDict();
  EXPECT_FALSE(result.contains(kFragmentPrefixKey));
  EXPECT_EQ("start", *result.FindString(kFragmentTextStartKey));
  EXPECT_FALSE(result.contains(kFragmentTextEndKey));
  EXPECT_FALSE(result.contains(kFragmentSuffixKey));

  // Failure Cases
  fragment = "";
  base::Value result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  fragment = "some,really-,malformed,-thing,with,too,many,commas";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  fragment = "prefix-,-suffix";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  fragment = "start,prefix-,-suffix";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  fragment = "prefix-,-suffix,start";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  fragment = "prefix-";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  fragment = "-suffix";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());

  // Invalid characters
  fragment = "\xFF\xFF";
  result_val = TextFragmentToValue(fragment);
  EXPECT_EQ(base::Value::Type::NONE, result_val.type());
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

TEST(TextFragmentTest, FromValue) {
  const char text_start[] = "test text start, * - &";
  const char text_end[] = "test text end, * - &";
  const char prefix[] = "prefix, * - &";
  const char suffix[] = "suffix, * - &";

  base::Value fragment_value = base::Value(base::Value::Type::DICT);

  // Empty value cases.
  EXPECT_FALSE(TextFragment::FromValue(&fragment_value).has_value());
  EXPECT_FALSE(TextFragment::FromValue(nullptr).has_value());
  base::Value string_value = base::Value(base::Value::Type::STRING);
  EXPECT_FALSE(TextFragment::FromValue(&string_value).has_value());

  fragment_value.GetDict().Set(kFragmentTextStartKey, text_start);
  fragment_value.GetDict().Set(kFragmentTextEndKey, text_end);
  fragment_value.GetDict().Set(kFragmentPrefixKey, prefix);
  fragment_value.GetDict().Set(kFragmentSuffixKey, suffix);

  std::optional<TextFragment> opt_fragment =
      TextFragment::FromValue(&fragment_value);
  EXPECT_TRUE(opt_fragment.has_value());
  TextFragment fragment = opt_fragment.value();
  EXPECT_EQ(text_start, fragment.text_start());
  EXPECT_EQ(text_end, fragment.text_end());
  EXPECT_EQ(prefix, fragment.prefix());
  EXPECT_EQ(suffix, fragment.suffix());

  // Testing the case where the dictionary value doesn't have a text start
  // value.
  ASSERT_TRUE(fragment_value.GetDict().Remove(kFragmentTextStartKey));
  EXPECT_FALSE(TextFragment::FromValue(&fragment_value).has_value());
}

}  // namespace
}  // namespace shared_highlighting
