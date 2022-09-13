// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/language_prefs_test_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAreArray;

namespace language {

class LanguagePrefsTest : public testing::Test {
 protected:
  LanguagePrefsTest()
      : prefs_(new sync_preferences::TestingPrefServiceSyncable()) {
    LanguagePrefs::RegisterProfilePrefs(prefs_->registry());
    language_prefs_ = std::make_unique<language::LanguagePrefs>(prefs_.get());
  }

  void SetUp() override {
    prefs_->SetString(language::prefs::kAcceptLanguages, std::string());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    prefs_->SetString(language::prefs::kPreferredLanguages, std::string());
#endif
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  std::unique_ptr<language::LanguagePrefs> language_prefs_;
};

TEST_F(LanguagePrefsTest, GetFirstLanguageTest) {
  EXPECT_EQ("a", language::GetFirstLanguage("a,b,c"));
  EXPECT_EQ("en-US", language::GetFirstLanguage("en-US,en,en-GB"));
  EXPECT_EQ("en-US", language::GetFirstLanguage("en-US"));
  EXPECT_EQ("", language::GetFirstLanguage(""));
}

TEST_F(LanguagePrefsTest, UpdateLanguageList) {
  language::test::LanguagePrefTester content_languages_tester =
      language::test::LanguagePrefTester(prefs_.get());
  // Empty update.
  std::vector<std::string> languages;
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("");

  // One language.
  languages = {"en"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("en");

  // More than one language.
  languages = {"en", "ja", "it"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("en,ja,it");

  // Locale-specific codes.
  languages = {"en-US", "ja", "en-CA", "fr-CA"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("en-US,ja,en-CA,fr-CA");
}

TEST_F(LanguagePrefsTest, UpdateForcedLanguageList) {
  // Only test policy-forced languages on non-Chrome OS platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  GTEST_SKIP();
#else
  language::test::LanguagePrefTester content_languages_tester =
      language::test::LanguagePrefTester(prefs_.get());
  // Empty update.
  std::vector<std::string> languages;
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("");

  // Forced languages with no duplicates.
  languages = {"fr"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,it,fr");
  content_languages_tester.SetForcedLanguagePrefs({});  // Reset pref

  // Forced languages with some duplicates.
  languages = {"en-US", "en", "fr", "fr-CA"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en-US,en,fr,fr-CA");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,it,en-US,fr,fr-CA");
  content_languages_tester.SetForcedLanguagePrefs({});  // Reset pref

  // Forced languages with full duplicates.
  languages = {"en", "es", "fr"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "es", "fr"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,es,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,es,fr");
  content_languages_tester.SetForcedLanguagePrefs({});  // Reset pref

  // Add then remove forced languages with no duplicates.
  languages = {"en", "fr"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("it,en,fr");
  // Remove forced languages
  content_languages_tester.SetForcedLanguagePrefs({});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");

  // Add then remove forced languages with some duplicates.
  languages = {"en", "fr"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,it,fr");
  // Remove forced languages
  content_languages_tester.SetForcedLanguagePrefs({});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");

  // Add then remove forced languages with full duplicates.
  languages = {"en", "fr"};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "fr"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");
  // Remove forced languages
  content_languages_tester.SetForcedLanguagePrefs({});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");
#endif
}

TEST_F(LanguagePrefsTest, ResetLanguagePrefs) {
  language::test::LanguagePrefTester content_languages_tester =
      language::test::LanguagePrefTester(prefs_.get());

  language_prefs_->SetUserSelectedLanguagesList({"en", "es", "fr"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,es,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,es,fr");
#if BUILDFLAG(IS_ANDROID)
  language_prefs_->SetULPLanguages({"a", "b", "c"});
  EXPECT_THAT(language_prefs_->GetULPLanguages(),
              testing::ElementsAre("a", "b", "c"));
#endif

  ResetLanguagePrefs(prefs_.get());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(language_prefs_->GetULPLanguages(), testing::IsEmpty());
#endif
  content_languages_tester.ExpectSelectedLanguagePrefs("");
  // Accept languages pref is reset to the default value, not cleared.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  content_languages_tester.ExpectAcceptLanguagePrefs(
      prefs_->GetDefaultPrefValue(language::prefs::kPreferredLanguages)
          ->GetString());
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  content_languages_tester.ExpectAcceptLanguagePrefs(
      prefs_->GetDefaultPrefValue(language::prefs::kAcceptLanguages)
          ->GetString());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST_F(LanguagePrefsTest, ULPLanguagesPref) {
#if BUILDFLAG(IS_ANDROID)
  // ULPLanguagesPref is initially empty.
  EXPECT_THAT(language_prefs_->GetULPLanguages(), testing::IsEmpty());

  // Set ULP Language Preference.
  language_prefs_->SetULPLanguages({"a", "b", "c"});
  EXPECT_THAT(language_prefs_->GetULPLanguages(),
              testing::ElementsAre("a", "b", "c"));

  // Setting ULP languages to a new list clears the old list.
  language_prefs_->SetULPLanguages({"d", "e", "f"});
  EXPECT_THAT(language_prefs_->GetULPLanguages(),
              testing::ElementsAre("d", "e", "f"));

  // Setting ULP languages to a an empty list clears it.
  language_prefs_->SetULPLanguages({});
  EXPECT_THAT(language_prefs_->GetULPLanguages(), testing::IsEmpty());
#endif
}
}  // namespace language
