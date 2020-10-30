// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/shared_highlighting/core/common/text_fragment.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace shared_highlighting {
namespace {

TEST(TextFragmentTest, FragmentToStringEmpty) {
  EXPECT_EQ("", TextFragment("").ToString());
}

TEST(TextFragmentTest, FragmentToStringEmptyTextStart) {
  EXPECT_EQ("", TextFragment("", "a", "b", "c").ToString());
}

TEST(TextFragmentTest, FragmentToStringOnlyTextStart) {
  EXPECT_EQ("text=only%20start", TextFragment("only start").ToString());
}

TEST(TextFragmentTest, FragmentToStringWithTextEnd) {
  EXPECT_EQ("text=only%20start,and%20end",
            TextFragment("only start", "and end", "", "").ToString());
}

TEST(TextFragmentTest, FragmentToStringWithPrefix) {
  EXPECT_EQ("text=and%20prefix-,only%20start",
            TextFragment("only start", "", "and prefix", "").ToString());
}

TEST(TextFragmentTest, FragmentToStringWithPrefixAndSuffix) {
  EXPECT_EQ(
      "text=and%20prefix-,only%20start,-and%20suffix",
      TextFragment("only start", "", "and prefix", "and suffix").ToString());
}

TEST(TextFragmentTest, FragmentToStringAllWithSpecialCharacters) {
  TextFragment test_fragment("text, Start-&", "end of, & Text-", "pre-fix&, !",
                             "suff,i,x-+&");
  EXPECT_EQ(
      "text=pre%2Dfix%26%2C%20!-,"
      "text%2C%20Start%2D%26"
      ",end%20of%2C%20%26%20Text%2D"
      ",-suff%2Ci%2Cx%2D%2B%26",
      test_fragment.ToString());
}

}  // namespace
}  // namespace shared_highlighting
