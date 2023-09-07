// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/language_state.h"

#include "components/translate/core/browser/mock_translate_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using translate::testing::MockTranslateDriver;

namespace translate {

TEST(LanguageStateTest, IsPageTranslated) {
  MockTranslateDriver driver;
  LanguageState language_state(&driver);
  EXPECT_FALSE(language_state.IsPageTranslated());

  // Navigate to a French page.
  language_state.LanguageDetermined("fr", true);
  EXPECT_EQ("fr", language_state.source_language());
  EXPECT_EQ("fr", language_state.current_language());
  EXPECT_FALSE(language_state.IsPageTranslated());

  // Translate the page into English.
  language_state.SetCurrentLanguage("en");
  EXPECT_EQ("fr", language_state.source_language());
  EXPECT_EQ("en", language_state.current_language());
  EXPECT_TRUE(language_state.IsPageTranslated());

  // Move on another page in Japanese.
  language_state.LanguageDetermined("ja", true);
  EXPECT_EQ("ja", language_state.source_language());
  EXPECT_EQ("ja", language_state.current_language());
  EXPECT_FALSE(language_state.IsPageTranslated());
}

TEST(LanguageStateTest, SetPredefinedTargetLanguage) {
  MockTranslateDriver driver;
  LanguageState language_state(&driver);

  // Language codes that do not have Translate synonyms.
  language_state.SetPredefinedTargetLanguage("fr", false);
  EXPECT_EQ("fr", language_state.GetPredefinedTargetLanguage());

  language_state.SetPredefinedTargetLanguage("sw", false);
  EXPECT_EQ("sw", language_state.GetPredefinedTargetLanguage());

  // Check that country codes are only preserved for "zh"
  language_state.SetPredefinedTargetLanguage("fr-CA", false);
  EXPECT_EQ("fr", language_state.GetPredefinedTargetLanguage());

  language_state.SetPredefinedTargetLanguage("zh-HK", false);
  EXPECT_EQ("zh-TW", language_state.GetPredefinedTargetLanguage());

  // Language codes that have Translate synonyms.
  language_state.SetPredefinedTargetLanguage("fil", false);
  EXPECT_EQ("tl", language_state.GetPredefinedTargetLanguage());

  language_state.SetPredefinedTargetLanguage("he", false);
  EXPECT_EQ("iw", language_state.GetPredefinedTargetLanguage());
}

TEST(LanguageStateTest, Driver) {
  MockTranslateDriver driver;
  LanguageState language_state(&driver);

  // Enable/Disable translate.
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_FALSE(driver.on_translate_enabled_changed_called());
  language_state.SetTranslateEnabled(true);
  EXPECT_TRUE(language_state.translate_enabled());
  EXPECT_TRUE(driver.on_translate_enabled_changed_called());

  driver.Reset();
  language_state.SetTranslateEnabled(false);
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_TRUE(driver.on_translate_enabled_changed_called());

  // Navigate to a French page.
  driver.Reset();
  language_state.LanguageDetermined("fr", true);
  EXPECT_FALSE(language_state.translate_enabled());
  EXPECT_FALSE(driver.on_is_page_translated_changed_called());
  EXPECT_FALSE(driver.on_translate_enabled_changed_called());

  // Translate.
  language_state.SetCurrentLanguage("en");
  EXPECT_TRUE(language_state.IsPageTranslated());
  EXPECT_TRUE(driver.on_is_page_translated_changed_called());

  // Translate feature must be enabled after an actual translation.
  EXPECT_TRUE(language_state.translate_enabled());
  EXPECT_TRUE(driver.on_translate_enabled_changed_called());
}

}  // namespace translate
