// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/language_state.h"

#include "base/i18n/language_tag.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using translate::testing::MockTranslateDriver;

namespace translate {

using ::base::i18n::GetKnownLanguageTag;

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
  EXPECT_EQ("fil", language_state.GetPredefinedTargetLanguage());

  language_state.SetPredefinedTargetLanguage("he", false);
  EXPECT_EQ("he", language_state.GetPredefinedTargetLanguage());
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

TEST(LanguageStateTest, PendingTranslation) {
  MockTranslateDriver driver;
  LanguageState language_state(&driver);

  // Initial state.
  EXPECT_FALSE(language_state.translation_pending());
  EXPECT_FALSE(language_state.pending_source_language().has_value());
  EXPECT_FALSE(language_state.pending_target_language().has_value());

  // Set pending translation languages.
  language_state.set_translation_pending(true);
  language_state.SetPendingTranslationLanguages(GetKnownLanguageTag("fr"),
                                                GetKnownLanguageTag("en"));
  EXPECT_TRUE(language_state.translation_pending());
  EXPECT_EQ(language_state.pending_source_language(), GetKnownLanguageTag("fr"));
  EXPECT_EQ(language_state.pending_target_language(), GetKnownLanguageTag("en"));

  // Setting translation_pending to false should clear pending languages.
  language_state.set_translation_pending(false);
  EXPECT_FALSE(language_state.translation_pending());
  EXPECT_FALSE(language_state.pending_source_language().has_value());
  EXPECT_FALSE(language_state.pending_target_language().has_value());

  // Set again, and verify that navigating clears the pending state and languages.
  language_state.set_translation_pending(true);
  language_state.SetPendingTranslationLanguages(GetKnownLanguageTag("fr"),
                                                GetKnownLanguageTag("en"));
  language_state.DidNavigate(/*is_same_document_navigation=*/false,
                             /*is_main_frame=*/true, /*reload=*/false,
                             /*href_translate=*/"",
                             /*navigation_from_google=*/false);
  EXPECT_FALSE(language_state.translation_pending());
  EXPECT_FALSE(language_state.pending_source_language().has_value());
  EXPECT_FALSE(language_state.pending_target_language().has_value());
}

TEST(LanguageStateTest, PdfTranslatability) {
  MockTranslateDriver driver;
  LanguageState language_state(&driver);

  // Initial state.
  EXPECT_EQ(LanguageState::PdfTranslatabilityStatus::kNotChecked,
            language_state.pdf_translatability_status());

  // Set to translatable.
  language_state.set_pdf_translatability_status(
      LanguageState::PdfTranslatabilityStatus::kTranslatable);
  EXPECT_EQ(LanguageState::PdfTranslatabilityStatus::kTranslatable,
            language_state.pdf_translatability_status());

  // Same-document navigation does not reset status.
  language_state.DidNavigate(/*is_same_document_navigation=*/true,
                             /*is_main_frame=*/true, /*reload=*/false,
                             /*href_translate=*/"",
                             /*navigation_from_google=*/false);
  EXPECT_EQ(LanguageState::PdfTranslatabilityStatus::kTranslatable,
            language_state.pdf_translatability_status());

  // Main frame navigation resets status.
  language_state.DidNavigate(/*is_same_document_navigation=*/false,
                             /*is_main_frame=*/true, /*reload=*/false,
                             /*href_translate=*/"",
                             /*navigation_from_google=*/false);
  EXPECT_EQ(LanguageState::PdfTranslatabilityStatus::kNotChecked,
            language_state.pdf_translatability_status());
}

}  // namespace translate

