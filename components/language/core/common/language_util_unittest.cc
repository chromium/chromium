// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/common/language_util.h"

#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test LanguageUtilTest;

// Tests that synonym language code is converted to one used in supporting list.
TEST_F(LanguageUtilTest, ToTranslateLanguageSynonym) {
  std::string language;

  language = std::string("nb");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("no", language);

  language = std::string("in");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("id", language);

  language = std::string("fil");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("tl", language);

  // Test all known Chinese cases.
  language = std::string("zh-HK");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("zh-TW", language);
  language = std::string("zh-MO");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("zh-TW", language);
  language = std::string("zh-SG");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("zh-CN", language);
  language = std::string("zh");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("zh", language);

  // A sub code is not preserved (except for Chinese).
  language = std::string("he-IL");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("iw", language);

  language = std::string("nb-NO");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("no", language);

  language = std::string("zh-JP");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("zh-JP", language);

  // Preserve the argument if it doesn't have its synonym.
  language = std::string("en");
  language::ToTranslateLanguageSynonym(&language);
  EXPECT_EQ("en", language);
}

// Tests that synonym language code is converted to one used in Chrome internal.
TEST_F(LanguageUtilTest, ToChromeLanguageSynonym) {
  std::string language;

  // Norwegian (no) and Norwegian Bokmal (nb) are both supported.
  language = std::string("no");
  language::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("no", language);

  language = std::string("nb");
  language::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("nb", language);

  // Convert to Chrome synonym
  language = std::string("tl");
  language::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("fil", language);

  // Preserve a sub code
  language = std::string("iw-IL");
  language::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("he-IL", language);

  // Preserve the argument if it doesn't have its synonym.
  language = std::string("en");
  language::ToChromeLanguageSynonym(&language);
  EXPECT_EQ("en", language);
}
