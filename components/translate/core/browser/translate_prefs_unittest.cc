// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_prefs.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/language_prefs_test_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/translate_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace translate {

namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAreArray;

static void ExpectEqualLanguageLists(
    const base::Value::List& language_values,
    const std::vector<std::string>& languages) {
  const int input_size = languages.size();
  ASSERT_EQ(input_size, static_cast<int>(language_values.size()));
  for (int i = 0; i < input_size; ++i) {
    ASSERT_TRUE(language_values[i].is_string());
    EXPECT_EQ(languages[i], language_values[i].GetString());
  }
}

}  // namespace

class TranslatePrefsTest : public testing::Test {
 protected:
  TranslatePrefsTest() {
    language::LanguagePrefs::RegisterProfilePrefs(prefs_.registry());
    TranslatePrefs::RegisterProfilePrefs(prefs_.registry());
    translate_prefs_ = std::make_unique<TranslatePrefs>(&prefs_);
    accept_languages_tester_ =
        std::make_unique<language::test::LanguagePrefTester>(&prefs_);
    now_ = base::Time::Now();
    two_days_ago_ = now_ - base::Days(2);
  }

  void SetUp() override {
    prefs_.SetString(language::prefs::kAcceptLanguages, std::string());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    prefs_.SetString(language::prefs::kPreferredLanguages, std::string());
#endif
    prefs_.registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true,
        user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  }

  void ExpectBlockedLanguageListContent(
      const std::vector<std::string>& list) const {
    const base::Value::List& never_prompt_list =
        prefs_.GetList(prefs::kBlockedLanguages);
    ExpectEqualLanguageLists(never_prompt_list, list);
  }

  // Returns a vector of language codes from the elements of the given
  // |language_list|.
  std::vector<std::string> ExtractLanguageCodes(
      const std::vector<TranslateLanguageInfo>& language_list) const {
    std::vector<std::string> output;
    for (const auto& item : language_list) {
      output.push_back(item.code);
    }
    return output;
  }

  // Returns a vector of display names from the elements of the given
  // |language_list|.
  std::vector<std::u16string> ExtractDisplayNames(
      const std::vector<TranslateLanguageInfo>& language_list) const {
    std::vector<std::u16string> output;
    for (const auto& item : language_list) {
      output.push_back(base::UTF8ToUTF16(item.display_name));
    }
    return output;
  }

  // Finds and returns the element in |language_list| that has the given code.
  TranslateLanguageInfo GetLanguageByCode(
      const std::string& language_code,
      const std::vector<TranslateLanguageInfo>& language_list) const {
    // Perform linear search as we don't care much about efficiency in test.
    // The size of the vector is ~150.
    TranslateLanguageInfo result;
    for (const auto& i : language_list) {
      if (language_code == i.code) {
        result = i;
      }
    }
    return result;
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  std::unique_ptr<TranslatePrefs> translate_prefs_;
  std::unique_ptr<language::test::LanguagePrefTester> accept_languages_tester_;

  // Shared time constants.
  base::Time now_;
  base::Time two_days_ago_;
};

// Test that GetLanguageInfoList() returns the correct list of languages based
// on the given locale.
TEST_F(TranslatePrefsTest, GetLanguageInfoListCorrectLocale) {
  std::vector<TranslateLanguageInfo> language_list;
  std::vector<std::string> expected_codes;

  l10n_util::GetAcceptLanguagesForLocale("en-US", &expected_codes);
  TranslatePrefs::GetLanguageInfoList("en-US", true /* translate_allowed */,
                                      &language_list);
  std::vector<std::string> codes = ExtractLanguageCodes(language_list);
  EXPECT_THAT(codes, UnorderedElementsAreArray(expected_codes));

  language_list.clear();
  expected_codes.clear();
  codes.clear();
  l10n_util::GetAcceptLanguagesForLocale("ja", &expected_codes);
  TranslatePrefs::GetLanguageInfoList("ja", true /* translate_allowed */,
                                      &language_list);
  codes = ExtractLanguageCodes(language_list);
  EXPECT_THAT(codes, UnorderedElementsAreArray(expected_codes));

  language_list.clear();
  expected_codes.clear();
  codes.clear();
  l10n_util::GetAcceptLanguagesForLocale("es-AR", &expected_codes);
  TranslatePrefs::GetLanguageInfoList("es-AR", true /* translate_allowed */,
                                      &language_list);
  codes = ExtractLanguageCodes(language_list);
  EXPECT_THAT(codes, UnorderedElementsAreArray(expected_codes));
}

// Check the output of GetLanguageInfoList().
TEST_F(TranslatePrefsTest, GetLanguageInfoListOutput) {
  std::vector<TranslateLanguageInfo> language_list;

  // Empty locale returns empty output.
  TranslatePrefs::GetLanguageInfoList("", true /* translate_allowed */,
                                      &language_list);
  EXPECT_TRUE(language_list.empty());

  // Output is sorted.
  language_list.clear();
  TranslatePrefs::GetLanguageInfoList("en-US", true /* translate_allowed */,
                                      &language_list);
  const std::vector<std::u16string> display_names =
      ExtractDisplayNames(language_list);
  std::vector<std::u16string> sorted(display_names);
  l10n_util::SortVectorWithStringKey("en-US", &sorted, false);

  EXPECT_THAT(display_names, ElementsAreArray(sorted));
}

TEST_F(TranslatePrefsTest, GetLanguageInfoList) {
  std::vector<TranslateLanguageInfo> language_list;
  TranslateLanguageInfo language;

  TranslatePrefs::GetLanguageInfoList("en-US", true /* translate_allowed */,
                                      &language_list);

  language = GetLanguageByCode("en", language_list);
  EXPECT_EQ("en", language.code);
  EXPECT_TRUE(language.supports_translate);

  language = GetLanguageByCode("en-US", language_list);
  EXPECT_EQ("en-US", language.code);
  EXPECT_TRUE(language.supports_translate);

  language = GetLanguageByCode("it", language_list);
  EXPECT_EQ("it", language.code);
  EXPECT_TRUE(language.supports_translate);

  language = GetLanguageByCode("it-IT", language_list);
  EXPECT_EQ("it-IT", language.code);
  EXPECT_TRUE(language.supports_translate);

  language = GetLanguageByCode("zh-HK", language_list);
  EXPECT_EQ("zh-HK", language.code);
  EXPECT_TRUE(language.supports_translate);
}

// Test that GetTranslatableContentLanguages() returns the correct list.
TEST_F(TranslatePrefsTest, GetTranslatableContentLanguagesCorrectLocale) {
  std::vector<std::string> result_codes;

  std::vector<std::string> content_languages;
  std::vector<std::string> expected_translatable_codes;

  // Set content languages.
  content_languages = {"en"};
  expected_translatable_codes = {"en"};
  accept_languages_tester_->SetLanguagePrefs(content_languages);

  // Empty locale returns empty output.
  translate_prefs_->GetTranslatableContentLanguages("", &result_codes);
  EXPECT_TRUE(result_codes.empty());

  translate_prefs_->GetTranslatableContentLanguages("en-US", &result_codes);
  EXPECT_THAT(expected_translatable_codes, result_codes);

  // Set content languages. Waloon ("wa") is not translatable and shouldn't
  // be included in the list.
  content_languages = {"ja", "en", "en-US", "wa"};
  expected_translatable_codes = {"ja", "en"};
  accept_languages_tester_->SetLanguagePrefs(content_languages);
  translate_prefs_->GetTranslatableContentLanguages("ja", &result_codes);
  EXPECT_THAT(result_codes, expected_translatable_codes);

  // Test with only untranslatable languages.
  content_languages = {"wa", "vo"};
  expected_translatable_codes = {};
  accept_languages_tester_->SetLanguagePrefs(content_languages);

  translate_prefs_->GetTranslatableContentLanguages("en-US", &result_codes);
  EXPECT_THAT(expected_translatable_codes, result_codes);

  // Verify that language codes are translated from Chrome to Translate format.
  content_languages = {"en", "nb", "zh-HK"};
  expected_translatable_codes = {"en", "no", "zh-TW"};
  accept_languages_tester_->SetLanguagePrefs(content_languages);
  translate_prefs_->GetTranslatableContentLanguages("ja", &result_codes);
  EXPECT_THAT(result_codes, expected_translatable_codes);
}

TEST_F(TranslatePrefsTest, BlockLanguage) {
  // `en` is a default blocked language, it should be present already.
  ExpectBlockedLanguageListContent({"en"});

  // One language.
  translate_prefs_->BlockLanguage("fr-CA");
  ExpectBlockedLanguageListContent({"en", "fr"});

  // Add a few more.
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->BlockLanguage("de-de");
  ExpectBlockedLanguageListContent({"en", "fr", "es", "de"});

  // Add a duplicate.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->BlockLanguage("es-AR");
  ExpectBlockedLanguageListContent({"en", "es"});

  // Two languages with the same base.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("fr-CA");
  translate_prefs_->BlockLanguage("fr-FR");
  ExpectBlockedLanguageListContent({"en", "fr"});

  // Chinese is a special case.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("zh-MO");
  translate_prefs_->BlockLanguage("zh-CN");
  ExpectBlockedLanguageListContent({"en", "zh-TW", "zh-CN"});

  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("zh-TW");
  translate_prefs_->BlockLanguage("zh-HK");
  ExpectBlockedLanguageListContent({"en", "zh-TW"});
}

TEST_F(TranslatePrefsTest, UnblockLanguage) {
  // Language in the list.
  // Should not unblock last language.
  translate_prefs_->UnblockLanguage("en-UK");
  ExpectBlockedLanguageListContent({"en"});

  // Language in the list but with different region.
  // Should not unblock last language.
  translate_prefs_->UnblockLanguage("en-AU");
  ExpectBlockedLanguageListContent({"en"});

  // Language in the list.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("fr");
  translate_prefs_->UnblockLanguage("en-UK");
  ExpectBlockedLanguageListContent({"fr"});

  // Language in the list but with different region.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("fr");
  translate_prefs_->UnblockLanguage("en-AU");
  ExpectBlockedLanguageListContent({"fr"});

  // Multiple languages.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("fr-CA");
  translate_prefs_->BlockLanguage("fr-FR");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->UnblockLanguage("fr-FR");
  ExpectBlockedLanguageListContent({"en", "es"});

  // Chinese is a special case.
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("zh-MO");
  translate_prefs_->BlockLanguage("zh-CN");
  translate_prefs_->UnblockLanguage("zh-TW");
  ExpectBlockedLanguageListContent({"en", "zh-CN"});

  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("zh-MO");
  translate_prefs_->BlockLanguage("zh-CN");
  translate_prefs_->UnblockLanguage("zh-CN");
  ExpectBlockedLanguageListContent({"en", "zh-TW"});
}

TEST_F(TranslatePrefsTest, ResetEmptyBlockedLanguagesToDefaultTest) {
  ExpectBlockedLanguageListContent({"en"});

  translate_prefs_->ResetEmptyBlockedLanguagesToDefaults();
  ExpectBlockedLanguageListContent({"en"});

  translate_prefs_->BlockLanguage("fr");
  translate_prefs_->ResetEmptyBlockedLanguagesToDefaults();
  ExpectBlockedLanguageListContent({"en", "fr"});

  prefs_.Set(translate::prefs::kBlockedLanguages,
             base::Value(base::Value::Type::LIST));
  ExpectBlockedLanguageListContent({});
  translate_prefs_->ResetEmptyBlockedLanguagesToDefaults();
  ExpectBlockedLanguageListContent({"en"});
}

TEST_F(TranslatePrefsTest, GetNeverTranslateLanguagesTest) {
  // Default Fluent language is "en".
  EXPECT_THAT(translate_prefs_->GetNeverTranslateLanguages(),
              ElementsAreArray({"en"}));

  // Add two languages with the same base.
  translate_prefs_->BlockLanguage("fr-FR");
  translate_prefs_->BlockLanguage("fr-CA");
  EXPECT_THAT(translate_prefs_->GetNeverTranslateLanguages(),
              ElementsAreArray({"en", "fr"}));

  // Add language that comes before English alphabetically. It should be
  // appended to the list.
  translate_prefs_->BlockLanguage("af");
  EXPECT_THAT(translate_prefs_->GetNeverTranslateLanguages(),
              ElementsAreArray({"en", "fr", "af"}));
}

TEST_F(TranslatePrefsTest, AddToLanguageList) {
  std::vector<std::string> languages;

  // Force blocked false, language not already in list.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->AddToLanguageList("it-IT", /*force_blocked=*/false);
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it-IT");
  ExpectBlockedLanguageListContent({"en", "it"});

  // Force blocked false, language from same family already in list.
  languages = {"en", "es-AR"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->AddToLanguageList("es-ES", /*force_blocked=*/false);
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,es-AR,es-ES");
  ExpectBlockedLanguageListContent({"en"});
}

TEST_F(TranslatePrefsTest, RemoveFromLanguageList) {
  std::vector<std::string> languages;
  // Unblock last language of a family.
  languages = {"en-US", "es-AR"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-AR");
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en-US");
  ExpectBlockedLanguageListContent({"en"});

  // Do not unblock if not the last language of a family.
  languages = {"en-US", "es-AR", "es-ES"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->ResetBlockedLanguagesToDefault();
  translate_prefs_->BlockLanguage("en-US");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->RemoveFromLanguageList("es-AR");
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en-US,es-ES");
  ExpectBlockedLanguageListContent({"en", "es"});
}

TEST_F(TranslatePrefsTest, RemoveFromLanguageListRemovesRemainingUnsupported) {
  // There needs to be an App Locale set to determine whether a language can be
  // an Accept Language or not.
  TranslateDownloadManager::GetInstance()->set_application_locale("en");
  std::vector<std::string> languages;
  languages = {"en", "en-US", "en-FOO"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,en-US,en-FOO");
  translate_prefs_->RemoveFromLanguageList("en-US");
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");
  translate_prefs_->RemoveFromLanguageList("en");
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");
}

TEST_F(TranslatePrefsTest, RemoveFromLanguageListClearsRecentLanguage) {
  // Unset the recent target language when the last language of the target
  // language family is removed
  std::vector<std::string> languages;
  languages = {"en", "en-US", "es-AR"};

  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->SetRecentTargetLanguage("es-AR");
  EXPECT_EQ("es", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->RemoveFromLanguageList("es-AR");
  EXPECT_EQ("", translate_prefs_->GetRecentTargetLanguage());

  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->SetRecentTargetLanguage("en-US");
  EXPECT_EQ("en", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->RemoveFromLanguageList("en");
  EXPECT_EQ("en", translate_prefs_->GetRecentTargetLanguage());
  translate_prefs_->RemoveFromLanguageList("en-US");
  EXPECT_EQ("", translate_prefs_->GetRecentTargetLanguage());
}

TEST_F(TranslatePrefsTest, MoveLanguageToTheTop) {
  std::vector<std::string> languages;
  std::string enabled;
  const int offset = 0;  // ignored

  // First we test all cases that result in no change.
  // The method needs to handle them gracefully and simply do no-op.

  // Empty language list.
  languages = {};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en-US", TranslatePrefs::kTop, offset,
                                      {"en-US"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");

  // Search for empty string.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kTop, offset, {"en"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // List of enabled languages is empty.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kTop, offset, {});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // Everything empty.
  languages = {""};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kTop, offset, {});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");

  // Only one element in the list.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kTop, offset,
                                      {"en-US"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // Element is already at the top.
  languages = {"en", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kTop, offset,
                                      {"en", "fr"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr");

  // Below we test cases that result in a valid rearrangement of the list.

  // The language is already at the top of the enabled languages, but not at the
  // top of the list: we still need to push it to the top.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kTop, offset,
                                      {"it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("it,en,fr,es");

  // Swap two languages.
  languages = {"en", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kTop, offset,
                                      {"en", "fr"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,en");

  // Language in the middle.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kTop, offset,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("it,en,fr,es");

  // Language at the bottom.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kTop, offset,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("es,en,fr,it");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kTop, offset,
                                      {"en", "fr", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("zh,en,fr,it,es");
}

TEST_F(TranslatePrefsTest, MoveLanguageUp) {
  std::vector<std::string> languages;
  std::string enabled;

  //---------------------------------------------------------------------------
  // First we test all cases that result in no change.
  // The method needs to handle them gracefully and simply do no-op.

  // Empty language list.
  languages = {};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en-US", TranslatePrefs::kUp, 1,
                                      {"en-US"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");

  // Search for empty string.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kUp, 1, {"en"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // List of enabled languages is empty.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kUp, 1, {});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // Everything empty.
  languages = {""};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kUp, 1, {});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");

  // Only one element in the list.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kUp, 1, {"en"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // Element is already at the top.
  languages = {"en", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kUp, 1,
                                      {"en", "fr"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr");

  // The language is at the top of the enabled languages.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kUp, 1,
                                      {"it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("it,en,fr,es");

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  //---------------------------------------------------------------------------
  // Move with policy-forced languages present.
  // Forced languages should always remain at the top of the languages list and
  // can't be reordered.
  // Only test on non-Chrome OS platforms.

  // Try moving forced language up.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kUp, 1,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es,zh");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Try moving forced/user-selected duplicate languages.
  languages = {"it", "es", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kUp, 1,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es");
  accept_languages_tester_->ExpectSelectedLanguagePrefs("it,fr,es");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Move top selected language up by 1.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kUp, 1,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es,zh");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Try moving top selected language up to top of all languages.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kUp, 2,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es,zh");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref
#endif

  //---------------------------------------------------------------------------
  // Below we test cases that result in a valid rearrangement of the list.
  // First we move by 1 position only.

  // Swap two languages.
  languages = {"en", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kUp, 1,
                                      {"en", "fr"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,en");

  // Language in the middle.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kUp, 1,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it,fr,es");

  // Language at the bottom.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kUp, 1,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,es,it");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp, 1,
                                      {"en", "fr", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,zh,fr,it,es");

  //---------------------------------------------------------------------------
  // Move by more than 1 position.

  // Move all the way to the top.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kUp, 3,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("es,en,fr,it,zh");

  // Move to the middle of the list.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kUp, 2,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,es,fr,it,zh");

  // Move up the last language.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp, 3,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,zh,fr,it,es");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp, 2,
                                      {"en", "fr", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,zh,fr,it,es");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp, 2,
                                      {"en", "fr", "it", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,zh,fr,it,es");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh", "de", "pt"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("de", TranslatePrefs::kUp, 3,
                                      {"it", "es", "zh", "de", "pt"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("de,en,fr,it,es,zh,pt");

  // If offset is too large, we effectively move to the top.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kUp, 7,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("es,en,fr,it,zh");

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  //---------------------------------------------------------------------------
  // Move with policy-forced languages present.
  // Only test on non-Chrome OS platforms.

  // Move bottom selected language to top of all languages.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp, 4,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,zh,it,es");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Move middle selected language to top of all languages.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("es", TranslatePrefs::kUp, 3,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,es,it,zh");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Moving selected language up should cause it to jump over hidden duplicate
  // languages within the kSelectedLanguages pref.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "es", "fr"});
  translate_prefs_->RearrangeLanguage("zh", TranslatePrefs::kUp, 1,
                                      {"en", "es", "fr", "it", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,es,fr,zh,it");
  accept_languages_tester_->ExpectSelectedLanguagePrefs("zh,it,es");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref
#endif
}

TEST_F(TranslatePrefsTest, MoveLanguageDown) {
  std::vector<std::string> languages;
  std::string enabled;

  //---------------------------------------------------------------------------
  // First we test all cases that result in no change.
  // The method needs to handle them gracefully and simply do no-op.

  // Empty language list.
  languages = {};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en-US", TranslatePrefs::kDown, 1,
                                      {"en-US"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");

  // Search for empty string.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kDown, 1, {"en"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // List of enabled languages is empty.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1, {});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // Everything empty.
  languages = {""};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("", TranslatePrefs::kDown, 1, {});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("");

  // Only one element in the list.
  languages = {"en"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1, {"en"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en");

  // Element is already at the bottom.
  languages = {"en", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 1,
                                      {"en", "fr"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr");

  // The language is at the bottom of the enabled languages: we move it to the
  // very bottom of the list.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kDown, 1,
                                      {"fr", "it"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,es,it");

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  //---------------------------------------------------------------------------
  // Move with policy-forced languages present.
  // Only test on non-Chrome OS platforms.

  // Try moving forced language down.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 1,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es,zh");
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es,zh");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Try moving forced/user-selected duplicate languages.
  languages = {"en", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "fr"});
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,fr,it,es");
  accept_languages_tester_->ExpectSelectedLanguagePrefs("it,en,es");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref

  // Moving selected language down should cause it to jump over hidden duplicate
  // languages within the kSelectedLanguages pref.
  languages = {"it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  accept_languages_tester_->SetForcedLanguagePrefs({"en", "es", "fr"});
  translate_prefs_->RearrangeLanguage("it", TranslatePrefs::kDown, 1,
                                      {"en", "es", "fr", "it", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,es,fr,zh,it");
  accept_languages_tester_->ExpectSelectedLanguagePrefs("es,zh,it");
  accept_languages_tester_->SetForcedLanguagePrefs({});  // Reset pref
#endif

  //---------------------------------------------------------------------------
  // Below we test cases that result in a valid rearrangement of the list.
  // First we move by 1 position only.

  // Swap two languages.
  languages = {"en", "fr"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1,
                                      {"en", "fr"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,en");

  // Language in the middle.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 1,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it,fr,es");

  // Language at the top.
  languages = {"en", "fr", "it", "es"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1,
                                      {"en", "fr", "it", "es"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,en,it,es");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 1,
                                      {"en", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,it,es,en,zh");

  //---------------------------------------------------------------------------
  // Move by more than 1 position.

  // Move all the way to the bottom.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 3,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it,es,zh,fr");

  // Move to the middle of the list.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 2,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it,es,fr,zh");

  // Move down the first language.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 3,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,it,es,en,zh");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 2,
                                      {"en", "fr", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,it,es,en,zh");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("en", TranslatePrefs::kDown, 2,
                                      {"en", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("fr,it,es,en,zh");

  // Skip languages that are not enabled.
  languages = {"en", "fr", "it", "es", "zh", "de", "pt"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 3,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it,es,zh,fr,de,pt");

  // If offset is too large, we effectively move to the bottom.
  languages = {"en", "fr", "it", "es", "zh"};
  accept_languages_tester_->SetLanguagePrefs(languages);
  translate_prefs_->RearrangeLanguage("fr", TranslatePrefs::kDown, 6,
                                      {"en", "fr", "it", "es", "zh"});
  accept_languages_tester_->ExpectAcceptLanguagePrefs("en,it,es,zh,fr");
}

TEST_F(TranslatePrefsTest, MigrateNeverPromptSites) {
  // Add two sites to the deprecated pref that need to be migrated.
  translate_prefs_->AddValueToNeverPromptList(
      TranslatePrefs::kPrefNeverPromptSitesDeprecated, "unmigrated.com");
  translate_prefs_->AddValueToNeverPromptList(
      TranslatePrefs::kPrefNeverPromptSitesDeprecated, "migratedWrong.com");
  EXPECT_EQ(
      prefs_.GetList(TranslatePrefs::kPrefNeverPromptSitesDeprecated).size(),
      2u);
  // Also put one of those sites on the new pref but migrated incorrectly.
  ScopedDictPrefUpdate never_prompt_list_update(
      &prefs_, prefs::kPrefNeverPromptSitesWithTime);
  base::Value::Dict& never_prompt_list = never_prompt_list_update.Get();
  never_prompt_list.Set("migratedWrong.com", 0);

  // Now migrate and fix the prefs.
  translate_prefs_->MigrateNeverPromptSites();
  EXPECT_THAT(translate_prefs_->GetNeverPromptSitesBetween(
                  base::Time::Now() - base::Days(1), base::Time::Max()),
              ElementsAre("migratedWrong.com", "unmigrated.com"));
  EXPECT_EQ(
      prefs_.GetList(TranslatePrefs::kPrefNeverPromptSitesDeprecated).size(),
      0u);
}

// Regression test for https://crbug.com/1295549
TEST_F(TranslatePrefsTest, InvalidNeverPromptSites) {
  // Add sites with invalid times.
  ScopedDictPrefUpdate never_prompt_list_update(
      &prefs_, prefs::kPrefNeverPromptSitesWithTime);
  base::Value::Dict& never_prompt_list = never_prompt_list_update.Get();
  never_prompt_list.Set("not-a-string.com", 0);
  never_prompt_list.Set("not-a-valid-time.com", "foo");
  // Add the null time (valid time).
  never_prompt_list.Set("null-time.com", "0");

  // This should not crash, and filter invalid times.
  EXPECT_THAT(translate_prefs_->GetNeverPromptSitesBetween(base::Time::Min(),
                                                           base::Time::Max()),
              ElementsAre("null-time.com"));
}

TEST_F(TranslatePrefsTest, MigrateInvalidNeverPromptSites) {
  ScopedListPrefUpdate update(&prefs_,
                              TranslatePrefs::kPrefNeverPromptSitesDeprecated);
  base::Value::List& never_prompt_list = update.Get();
  never_prompt_list.Append(1);
  never_prompt_list.Append("unmigrated.com");
  translate_prefs_->MigrateNeverPromptSites();
  EXPECT_THAT(translate_prefs_->GetNeverPromptSitesBetween(
                  base::Time::Now() - base::Days(1), base::Time::Max()),
              ElementsAre("unmigrated.com"));
}

TEST_F(TranslatePrefsTest, ShouldNotifyUponMigrateNeverPromptSites) {
  // Listen to pref changes.
  MockPrefChangeCallback observer(&prefs_);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(prefs::kPrefNeverPromptSitesWithTime, observer.GetCallback());

  ScopedListPrefUpdate update(&prefs_,
                              TranslatePrefs::kPrefNeverPromptSitesDeprecated);
  base::Value::List& never_prompt_list = update.Get();
  never_prompt_list.Append("unmigrated.com");

  EXPECT_CALL(observer, OnPreferenceChanged);
  translate_prefs_->MigrateNeverPromptSites();
}

TEST_F(TranslatePrefsTest,
       ShouldNotifyUponMigrateNeverPromptSitesForNonEmptyInitialValue) {
  {
    // Add initial values to kPrefNeverPromptSitesWithTime.
    ScopedDictPrefUpdate never_prompt_list_update(
        &prefs_, prefs::kPrefNeverPromptSitesWithTime);
    base::Value::Dict& never_prompt_list = never_prompt_list_update.Get();
    never_prompt_list.Set("migrated.com", base::TimeToValue(base::Time::Now()));
  }

  // Listen to pref changes.
  MockPrefChangeCallback observer(&prefs_);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(prefs::kPrefNeverPromptSitesWithTime, observer.GetCallback());

  ScopedListPrefUpdate update(&prefs_,
                              TranslatePrefs::kPrefNeverPromptSitesDeprecated);
  base::Value::List& never_prompt_list = update.Get();
  never_prompt_list.Append("unmigrated.com");

  EXPECT_CALL(observer, OnPreferenceChanged);
  translate_prefs_->MigrateNeverPromptSites();
  EXPECT_THAT(translate_prefs_->GetNeverPromptSitesBetween(
                  base::Time::Now() - base::Days(1), base::Time::Max()),
              ElementsAre("migrated.com", "unmigrated.com"));
}

TEST_F(TranslatePrefsTest, ShouldNotNotifyUponMigrateInvalidNeverPromptSites) {
  // Listen to pref changes.
  MockPrefChangeCallback observer(&prefs_);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(prefs::kPrefNeverPromptSitesWithTime, observer.GetCallback());

  ScopedListPrefUpdate update(&prefs_,
                              TranslatePrefs::kPrefNeverPromptSitesDeprecated);
  base::Value::List& never_prompt_list = update.Get();
  never_prompt_list.Append(1);

  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);
  translate_prefs_->MigrateNeverPromptSites();
}

TEST_F(TranslatePrefsTest, ShouldNotNotifyUponMigrateNoNeverPromptSites) {
  // Listen to pref changes.
  MockPrefChangeCallback observer(&prefs_);
  PrefChangeRegistrar registrar;
  registrar.Init(&prefs_);
  registrar.Add(prefs::kPrefNeverPromptSitesWithTime, observer.GetCallback());

  ASSERT_TRUE(
      prefs_.GetList(TranslatePrefs::kPrefNeverPromptSitesDeprecated).empty());

  EXPECT_CALL(observer, OnPreferenceChanged).Times(0);
  translate_prefs_->MigrateNeverPromptSites();
}

TEST_F(TranslatePrefsTest, SiteNeverPromptList) {
  base::Time a_insert = base::Time::Now();
  base::Time after_a_insert = a_insert + base::Seconds(2);
  base::Time b_insert = a_insert + base::Seconds(4);
  base::Time after_b_insert = a_insert + base::Seconds(6);
  translate_prefs_->AddSiteToNeverPromptList("a.com", a_insert);
  translate_prefs_->AddSiteToNeverPromptList("b.com", b_insert);
  EXPECT_TRUE(translate_prefs_->IsSiteOnNeverPromptList("a.com"));
  EXPECT_TRUE(translate_prefs_->IsSiteOnNeverPromptList("b.com"));

  EXPECT_EQ(std::vector<std::string>({"a.com"}),
            translate_prefs_->GetNeverPromptSitesBetween(base::Time(),
                                                         after_a_insert));
  EXPECT_EQ(std::vector<std::string>({"a.com", "b.com"}),
            translate_prefs_->GetNeverPromptSitesBetween(base::Time(),
                                                         after_b_insert));

  translate_prefs_->DeleteNeverPromptSitesBetween(after_a_insert,
                                                  base::Time::Max());
  EXPECT_TRUE(translate_prefs_->IsSiteOnNeverPromptList("a.com"));
  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList("b.com"));

  translate_prefs_->DeleteNeverPromptSitesBetween(base::Time(),
                                                  base::Time::Max());
  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList("a.com"));
  EXPECT_FALSE(translate_prefs_->IsSiteOnNeverPromptList("b.com"));
}

TEST_F(TranslatePrefsTest, DefaultBlockedLanguages) {
  translate_prefs_->ResetToDefaults();
  // The default blocked languages should be the unique language codes in the
  // default accept languages for Chrome (resource IDS_ACCEPT_LANGUAGES,
  // provided by components_locale_settings_en-US.pak), and
  // language::kFallbackInputMethodLocale for ChromeOS. For the tests, the
  // resources match.
  std::vector<std::string> blocked_languages_expected = {"en"};
  ExpectBlockedLanguageListContent(blocked_languages_expected);
}

TEST_F(TranslatePrefsTest, SetRecentTargetLanguage) {
  // Make sure setting the recent target language uses the Translate synonym.
  translate_prefs_->SetRecentTargetLanguage("en-US");
  EXPECT_EQ("en", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->SetRecentTargetLanguage("en-412");
  EXPECT_EQ("en", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->SetRecentTargetLanguage("fil");
  EXPECT_EQ("tl", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->SetRecentTargetLanguage("nb");
  EXPECT_EQ("no", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->SetRecentTargetLanguage("jv");
  EXPECT_EQ("jw", translate_prefs_->GetRecentTargetLanguage());

  translate_prefs_->SetRecentTargetLanguage("he");
  EXPECT_EQ("iw", translate_prefs_->GetRecentTargetLanguage());

  // The only translate languages to have a country code are variants of "zh".
  translate_prefs_->SetRecentTargetLanguage("zh-TW");
  EXPECT_EQ("zh-TW", translate_prefs_->GetRecentTargetLanguage());
}

// Series of tests for the AlwaysTranslateLanguagesList manipulation functions.
TEST_F(TranslatePrefsTest, AlwaysTranslateLanguages) {
  EXPECT_FALSE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());
  // Add translate language with country code.
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("af-ZA", "en-US");
  EXPECT_TRUE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());

  // IsLanguagePairOnAlwaysTranslateList
  EXPECT_TRUE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("af", "en"));
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("af", "es"));
  EXPECT_FALSE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("am", "en"));
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("am", "es");
  EXPECT_TRUE(
      translate_prefs_->IsLanguagePairOnAlwaysTranslateList("am", "es"));

  // GetAlwaysTranslateLanguages
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("ak", "es");
  // Use 'tl' as the translate language which is 'fil' as a Chrome language.
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("tl", "es");
  std::vector<std::string> always_translate_languages =
      translate_prefs_->GetAlwaysTranslateLanguages();
  EXPECT_EQ(std::vector<std::string>({"af", "ak", "am", "fil"}),
            always_translate_languages);
  always_translate_languages.clear();

  // RemoveLanguagePairs
  translate_prefs_->RemoveLanguagePairFromAlwaysTranslateList("af");
  always_translate_languages = translate_prefs_->GetAlwaysTranslateLanguages();
  EXPECT_EQ(std::vector<std::string>({"ak", "am", "fil"}),
            always_translate_languages);
  translate_prefs_->RemoveLanguagePairFromAlwaysTranslateList("ak");
  translate_prefs_->RemoveLanguagePairFromAlwaysTranslateList("am");
  translate_prefs_->RemoveLanguagePairFromAlwaysTranslateList("tl");

  // AlwaysTranslateList should be empty now
  EXPECT_FALSE(translate_prefs_->HasLanguagePairsToAlwaysTranslate());
}

// Test that a language can not be on both the never and always translate list.
TEST_F(TranslatePrefsTest, NeverOnAlwaysAndNever) {
  // "en" is a default blocked language, it should be present already.
  ExpectBlockedLanguageListContent({"en"});

  // Build up blocked language list to test removing languages.
  translate_prefs_->BlockLanguage("fr-CA");
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->BlockLanguage("de-de");
  ExpectBlockedLanguageListContent({"en", "fr", "es", "de"});

  // Add "fr" to always translate list.  Should remove from blocked list.
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("fr", "en");
  ExpectBlockedLanguageListContent({"en", "es", "de"});
  // Adding "es" as a target language does nothing.
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("af", "es");
  ExpectBlockedLanguageListContent({"en", "es", "de"});

  translate_prefs_->AddLanguagePairToAlwaysTranslateList("en", "hi");
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("es", "en");
  ExpectBlockedLanguageListContent({"de"});

  // Can not delete the last item from the blocked list.  In this case the
  // language will be on both list. (https://crbug.com/1196490).
  translate_prefs_->AddLanguagePairToAlwaysTranslateList("de", "en");
  ExpectBlockedLanguageListContent({"de"});

  // Check that the always translate list is what we expect.
  EXPECT_THAT(translate_prefs_->GetAlwaysTranslateLanguages(),
              ElementsAreArray({"af", "de", "en", "es", "fr"}));

  // Build up blocked language list and remove from always translate list.
  translate_prefs_->BlockLanguage("fr-CA");
  EXPECT_THAT(translate_prefs_->GetAlwaysTranslateLanguages(),
              ElementsAreArray({"af", "de", "en", "es"}));
  translate_prefs_->BlockLanguage("es-AR");
  translate_prefs_->BlockLanguage("de");
  translate_prefs_->BlockLanguage("af");
  EXPECT_THAT(translate_prefs_->GetAlwaysTranslateLanguages(),
              ElementsAreArray({"en"}));

  translate_prefs_->BlockLanguage("en");
  EXPECT_THAT(translate_prefs_->GetAlwaysTranslateLanguages(), IsEmpty());
}

TEST_F(TranslatePrefsTest, CanTranslateLanguage) {
  prefs_.SetString(language::prefs::kAcceptLanguages, "en");
  TranslateDownloadManager::GetInstance()->set_application_locale("en");

  translate_prefs_->ResetToDefaults();

  // Unblocked language.
  EXPECT_TRUE(translate_prefs_->CanTranslateLanguage("fr"));

  // Blocked language.
  translate_prefs_->BlockLanguage("en");
  EXPECT_FALSE(translate_prefs_->CanTranslateLanguage("en"));

  {  // English in force translate experiment scoped feature.
    base::test::ScopedFeatureList scoped_feature_list;
    translate_prefs_->SetShouldForceTriggerTranslateOnEnglishPagesForTesting();
    EXPECT_TRUE(translate_prefs_->CanTranslateLanguage("en"));
  }
}

TEST_F(TranslatePrefsTest, ForceTriggerOnEnglishPagesCount) {
  prefs_.SetInteger(TranslatePrefs::kPrefForceTriggerTranslateCount,
                    std::numeric_limits<int>::max() - 1);
  EXPECT_EQ(std::numeric_limits<int>::max() - 1,
            translate_prefs_->GetForceTriggerOnEnglishPagesCount());

  // The count should increment up to max int.
  translate_prefs_->ReportForceTriggerOnEnglishPages();
  EXPECT_EQ(std::numeric_limits<int>::max(),
            translate_prefs_->GetForceTriggerOnEnglishPagesCount());

  // The count should not increment past max int.
  translate_prefs_->ReportForceTriggerOnEnglishPages();
  EXPECT_EQ(std::numeric_limits<int>::max(),
            translate_prefs_->GetForceTriggerOnEnglishPagesCount());

  translate_prefs_->ReportAcceptedAfterForceTriggerOnEnglishPages();
  EXPECT_EQ(-1, translate_prefs_->GetForceTriggerOnEnglishPagesCount());

  // Incrementing after force triggering has already been accepted should have
  // no effect.
  translate_prefs_->ReportForceTriggerOnEnglishPages();
  EXPECT_EQ(-1, translate_prefs_->GetForceTriggerOnEnglishPagesCount());
}

class TranslatePrefsMigrationTest : public testing::Test {
 protected:
  TranslatePrefsMigrationTest() {
    language::LanguagePrefs::RegisterProfilePrefs(prefs_.registry());
    TranslatePrefs::RegisterProfilePrefs(prefs_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
};

TEST_F(TranslatePrefsMigrationTest,
       MigrateObsoleteAlwaysTranslateLanguagesPref_Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kMigrateAlwaysTranslateLanguagesFix);

  base::Value::List never_translate_list;
  never_translate_list.Append("en");

  base::Value::Dict old_always_translate_map;
  old_always_translate_map.Set("fr", "en");

  base::Value::Dict new_always_translate_map;
  new_always_translate_map.Set("ru", "en");

  prefs_.SetList(prefs::kBlockedLanguages, never_translate_list.Clone());
  prefs_.SetDict(TranslatePrefs::kPrefAlwaysTranslateListDeprecated,
                 old_always_translate_map.Clone());
  prefs_.SetDict(prefs::kPrefAlwaysTranslateList,
                 new_always_translate_map.Clone());

  // Since the kMigrateAlwaysTranslateLanguagesFix feature is disabled, no
  // migration should occur during construction.
  TranslatePrefs translate_prefs(&prefs_);

  EXPECT_EQ(prefs_.GetDict(TranslatePrefs::kPrefAlwaysTranslateListDeprecated),
            old_always_translate_map);
  EXPECT_EQ(prefs_.GetDict(prefs::kPrefAlwaysTranslateList),
            new_always_translate_map);
}

TEST_F(TranslatePrefsMigrationTest,
       MigrateObsoleteAlwaysTranslateLanguagesPref_Enabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      kMigrateAlwaysTranslateLanguagesFix);

  base::Value::List never_translate_list;
  never_translate_list.Append("en");
  never_translate_list.Append("es");
  prefs_.SetList(prefs::kBlockedLanguages, std::move(never_translate_list));

  base::Value::Dict old_always_translate_map;
  // A non-conflicting language pair that should be merged.
  old_always_translate_map.Set("fr", "en");
  // Conflicts with a new language pair with the same source language.
  old_always_translate_map.Set("ru", "de");
  // Conflicts with a new language pair with this source language as the target.
  old_always_translate_map.Set("jp", "de");
  // Conflicts with a new language pair with this target language as the source.
  old_always_translate_map.Set("pt", "hi");

  prefs_.SetDict(TranslatePrefs::kPrefAlwaysTranslateListDeprecated,
                 std::move(old_always_translate_map));

  base::Value::Dict new_always_translate_map;
  new_always_translate_map.Set("ru", "en");
  new_always_translate_map.Set("id", "jp");
  new_always_translate_map.Set("hi", "en");
  prefs_.SetDict(prefs::kPrefAlwaysTranslateList,
                 std::move(new_always_translate_map));

  // The always-translate pref migration should be done during construction.
  TranslatePrefs translate_prefs(&prefs_);

  EXPECT_FALSE(prefs_.GetUserPrefValue(
      TranslatePrefs::kPrefAlwaysTranslateListDeprecated));

  base::Value::Dict expected_always_translate_map;
  expected_always_translate_map.Set("ru", "en");
  expected_always_translate_map.Set("id", "jp");
  expected_always_translate_map.Set("hi", "en");
  expected_always_translate_map.Set("fr", "en");

  EXPECT_EQ(prefs_.GetDict(prefs::kPrefAlwaysTranslateList),
            expected_always_translate_map);
}

}  // namespace translate
