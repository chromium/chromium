// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_names_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ExpectNoParse(const std::string& path) {
  EXPECT_FALSE(pref_names_util::ParseFontNamePrefPath(path, nullptr, nullptr));
}

void ExpectParse(const std::string& path,
                 const std::string& expected_generic_family,
                 const std::string& expected_script)
{
  std::string generic_family;
  std::string script;

  ASSERT_TRUE(pref_names_util::ParseFontNamePrefPath(path, &generic_family,
                                                     &script));
  EXPECT_EQ(expected_generic_family, generic_family);
  EXPECT_EQ(expected_script, script);
}

}  // namespace

TEST(PrefNamesUtilTest, Basic) {
  ExpectNoParse(std::string());
  ExpectNoParse(".");
  ExpectNoParse(".....");
  ExpectNoParse("webkit.webprefs.fonts.");
  ExpectNoParse("webkit.webprefs.fonts..");
  ExpectNoParse("webkit.webprefs.fontsfoobar.standard.Hrkt");
  ExpectNoParse("foobar.webprefs.fonts.standard.Hrkt");
  ExpectParse("webkit.webprefs.fonts.standard.Hrkt", "standard", "Hrkt");
  ExpectParse("webkit.webprefs.fonts.standard.Hrkt.", "standard", "Hrkt.");
  ExpectParse("webkit.webprefs.fonts.standard.Hrkt.Foobar", "standard",
              "Hrkt.Foobar");

  // We don't particularly care about the parsed family and script for these
  // inputs, but just want to make sure it does something reasonable. Returning
  // false may also be an option.
  ExpectParse("webkit.webprefs.fonts...", std::string(), ".");
  ExpectParse("webkit.webprefs.fonts....", std::string(), "..");

  // Check that passing NULL output params is okay.
  EXPECT_TRUE(pref_names_util::ParseFontNamePrefPath(
      "webkit.webprefs.fonts.standard.Hrkt", nullptr, nullptr));
}
