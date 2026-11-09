// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_list_helper.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/language/core/browser/language_prefs_test_util.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

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
#if BUILDFLAG(IS_CHROMEOS)
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
  std::vector<LanguageTag> languages;
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("");

  // One language.
  languages = {GetKnownLanguageTag("en")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("en");

  // More than one language.
  languages = {GetKnownLanguageTag("en"), GetKnownLanguageTag("ja"),
               GetKnownLanguageTag("it")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("en,ja,it");

  // Locale-specific codes.
  languages = {GetKnownLanguageTag("en-US"), GetKnownLanguageTag("ja"),
               GetKnownLanguageTag("en-CA"), GetKnownLanguageTag("fr-CA")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("en-US,ja,en-CA,fr-CA");
}

TEST_F(LanguagePrefsTest, UpdateForcedLanguageList) {
  // Only test policy-forced languages on non-Chrome OS platforms.
#if BUILDFLAG(IS_CHROMEOS)
  GTEST_SKIP();
#else
  language::test::LanguagePrefTester content_languages_tester =
      language::test::LanguagePrefTester(prefs_.get());
  // Empty update.
  std::vector<LanguageTag> languages;
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.ExpectAcceptLanguagePrefs("");

  // Forced languages with no duplicates.
  languages = {GetKnownLanguageTag("fr")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,it,fr");
  content_languages_tester.SetForcedLanguagePrefs({});  // Reset pref

  // Forced languages with some duplicates.
  languages = {GetKnownLanguageTag("en-US"), GetKnownLanguageTag("en"),
               GetKnownLanguageTag("fr"), GetKnownLanguageTag("fr-CA")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en-US,en,fr,fr-CA");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,it,en-US,fr,fr-CA");
  content_languages_tester.SetForcedLanguagePrefs({});  // Reset pref

  // Forced languages with full duplicates.
  languages = {GetKnownLanguageTag("en"), GetKnownLanguageTag("es"),
               GetKnownLanguageTag("fr")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "es", "fr"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,es,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,es,fr");
  content_languages_tester.SetForcedLanguagePrefs({});  // Reset pref

  // Add then remove forced languages with no duplicates.
  languages = {GetKnownLanguageTag("en"), GetKnownLanguageTag("fr")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("it,en,fr");
  // Remove forced languages
  content_languages_tester.SetForcedLanguagePrefs({});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");

  // Add then remove forced languages with some duplicates.
  languages = {GetKnownLanguageTag("en"), GetKnownLanguageTag("fr")};
  language_prefs_->SetUserSelectedLanguagesList(languages);
  content_languages_tester.SetForcedLanguagePrefs({"en", "it"});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,it,fr");
  // Remove forced languages
  content_languages_tester.SetForcedLanguagePrefs({});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");

  // Add then remove forced languages with full duplicates.
  languages = {GetKnownLanguageTag("en"), GetKnownLanguageTag("fr")};
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

TEST_F(LanguagePrefsTest, GetUserSelectedLanguagesList) {
  language_prefs_->SetUserSelectedLanguagesList({});
  EXPECT_THAT(language_prefs_->GetUserSelectedLanguagesList(), IsEmpty());

  language_prefs_->SetUserSelectedLanguagesList(
      {GetKnownLanguageTag("en-US"), GetKnownLanguageTag("ja")});
  EXPECT_THAT(
      language_prefs_->GetUserSelectedLanguagesList(),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("ja")));

  // Invalid languages are skipped, and the other ones are canonicalized.
  prefs_->SetString(language::prefs::kSelectedLanguages,
                    "EN-us, ,invalid---tag,iw");
  EXPECT_THAT(
      language_prefs_->GetUserSelectedLanguagesList(),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("he")));
}

TEST_F(LanguagePrefsTest, GetAcceptLanguagesList) {
  language_prefs_->SetUserSelectedLanguagesList(
      {GetKnownLanguageTag("en-US"), GetKnownLanguageTag("ja")});
  EXPECT_THAT(
      language_prefs_->GetAcceptLanguagesList(),
      ElementsAre(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("ja")));

#if !BUILDFLAG(IS_CHROMEOS)
  // Policy-forced languages come first.
  language::test::LanguagePrefTester(prefs_.get())
      .SetForcedLanguagePrefs({"fr", "ja"});
  EXPECT_THAT(language_prefs_->GetAcceptLanguagesList(),
              ElementsAre(GetKnownLanguageTag("fr"), GetKnownLanguageTag("ja"),
                          GetKnownLanguageTag("en-US")));
#endif
}

TEST_F(LanguagePrefsTest, SetUserSelectedLanguagesListDropsNonAcceptLanguages) {
  language::test::LanguagePrefTester content_languages_tester =
      language::test::LanguagePrefTester(prefs_.get());
  // "aa" (Afar) can't be an accept language.
  language_prefs_->SetUserSelectedLanguagesList(
      base::i18n::GetLanguageTagListFromString("en,aa,fr"));
  content_languages_tester.ExpectSelectedLanguagePrefs("en,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,fr");
}

TEST_F(LanguagePrefsTest, AcceptLanguagesPrefOnlyContainsValidLanguages) {
  // Invalid languages are dropped, and the other ones are canonicalized.
  prefs_->SetString(language::prefs::kSelectedLanguages,
                    "EN-us, ,invalid---tag,iw");
  EXPECT_EQ(prefs_->GetString(language::prefs::kAcceptLanguages), "en-US,he");

  // Forced languages are deduplicated after being canonicalized.
  language::test::LanguagePrefTester(prefs_.get())
      .SetForcedLanguagePrefs({"en_us", "it", "EN-US", "invalid---tag"});
  EXPECT_EQ(prefs_->GetString(language::prefs::kAcceptLanguages),
            "en-US,it,he");
}

TEST_F(LanguagePrefsTest, IsForcedLanguage) {
  language::test::LanguagePrefTester(prefs_.get())
      .SetForcedLanguagePrefs({"en_US", "fr"});
  EXPECT_TRUE(language_prefs_->IsForcedLanguage(GetKnownLanguageTag("en-US")));
  EXPECT_TRUE(language_prefs_->IsForcedLanguage(GetKnownLanguageTag("fr")));
  EXPECT_FALSE(language_prefs_->IsForcedLanguage(GetKnownLanguageTag("en")));
  EXPECT_FALSE(language_prefs_->IsForcedLanguage(GetKnownLanguageTag("ja")));
}

TEST_F(LanguagePrefsTest, ResetLanguagePrefs) {
  language::test::LanguagePrefTester content_languages_tester =
      language::test::LanguagePrefTester(prefs_.get());

  language_prefs_->SetUserSelectedLanguagesList({GetKnownLanguageTag("en"),
                                                 GetKnownLanguageTag("es"),
                                                 GetKnownLanguageTag("fr")});
  content_languages_tester.ExpectSelectedLanguagePrefs("en,es,fr");
  content_languages_tester.ExpectAcceptLanguagePrefs("en,es,fr");
#if BUILDFLAG(IS_ANDROID)
  language_prefs_->SetULPLanguages({base::i18n::GetKnownLanguageTag("en"),
                                    base::i18n::GetKnownLanguageTag("es"),
                                    base::i18n::GetKnownLanguageTag("fr")});
  EXPECT_THAT(language_prefs_->GetULPLanguages(),
              ElementsAre(GetKnownLanguageTag("en"), GetKnownLanguageTag("es"),
                          GetKnownLanguageTag("fr")));
#endif

  ResetLanguagePrefs(prefs_.get());
#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(language_prefs_->GetULPLanguages(), IsEmpty());
#endif
  content_languages_tester.ExpectSelectedLanguagePrefs("");
  // Accept languages pref is reset to the default value, not cleared.
#if BUILDFLAG(IS_CHROMEOS)
  content_languages_tester.ExpectAcceptLanguagePrefs(
      prefs_->GetDefaultPrefValue(language::prefs::kPreferredLanguages)
          ->GetString());
#else   // BUILDFLAG(IS_CHROMEOS)
  content_languages_tester.ExpectAcceptLanguagePrefs(
      prefs_->GetDefaultPrefValue(language::prefs::kAcceptLanguages)
          ->GetString());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(LanguagePrefsTest, ULPLanguagesPref) {
#if BUILDFLAG(IS_ANDROID)
  // ULPLanguagesPref is initially empty.
  EXPECT_THAT(language_prefs_->GetULPLanguages(), IsEmpty());

  // Set ULP Language Preference.
  language_prefs_->SetULPLanguages({base::i18n::GetKnownLanguageTag("en"),
                                    base::i18n::GetKnownLanguageTag("es"),
                                    base::i18n::GetKnownLanguageTag("fr")});
  EXPECT_THAT(language_prefs_->GetULPLanguages(),
              ElementsAre(GetKnownLanguageTag("en"), GetKnownLanguageTag("es"),
                          GetKnownLanguageTag("fr")));

  // Setting ULP languages to a new list clears the old list.
  language_prefs_->SetULPLanguages({base::i18n::GetKnownLanguageTag("de"),
                                    base::i18n::GetKnownLanguageTag("pt"),
                                    base::i18n::GetKnownLanguageTag("zh")});
  EXPECT_THAT(language_prefs_->GetULPLanguages(),
              ElementsAre(GetKnownLanguageTag("de"), GetKnownLanguageTag("pt"),
                          GetKnownLanguageTag("zh")));

  // Setting ULP languages to a an empty list clears it.
  language_prefs_->SetULPLanguages({});
  EXPECT_THAT(language_prefs_->GetULPLanguages(), IsEmpty());
#endif
}
TEST_F(LanguagePrefsTest, GetIncognitoLanguageListTest) {
  // Test mapping from generated map.
  // For example "fr" should map to "fr-FR,fr,en-US,en" based on
  // components_locale_settings_fr.xtb.
  EXPECT_EQ("en-US,en", language::GetIncognitoLanguageList("en,fr"));
  EXPECT_EQ("en-US,en", language::GetIncognitoLanguageList("en-US,fr"));
  EXPECT_EQ("fr-FR,fr,en-US,en",
            language::GetIncognitoLanguageList("fr,en-US"));
  EXPECT_EQ("zh-CN,zh", language::GetIncognitoLanguageList("zh-CN,zh"));

  // Test fallback heuristic for language not in map.
  EXPECT_EQ("as,en-US,en", language::GetIncognitoLanguageList("as,en-US"));
  EXPECT_EQ("zz,en-US,en", language::GetIncognitoLanguageList("zz,en-US"));

  // Test empty or single language cases (should return unchanged).
  EXPECT_EQ("", language::GetIncognitoLanguageList(""));
  EXPECT_EQ("fr", language::GetIncognitoLanguageList("fr"));
  EXPECT_EQ("zh-CN", language::GetIncognitoLanguageList("zh-CN"));
  EXPECT_EQ("en", language::GetIncognitoLanguageList("en"));
  EXPECT_EQ("en-US", language::GetIncognitoLanguageList("en-US"));
}

TEST_F(LanguagePrefsTest, GetLanguageWithDefaultLimits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kReduceAcceptLanguageCount);

  EXPECT_EQ("en-US,ja,en-CA,fr-CA,zh-CN,en-GB,es,ru,pt,it",
            language::GetLanguagesWithMaxCount(
                "en-US,ja,en-CA,fr-CA,zh-CN,en-GB,es,ru,pt,it,ko"));
}

TEST_F(LanguagePrefsTest, GetLanguageWithCustomLimits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kReduceAcceptLanguageCount, {{kMaxAcceptLanguage.name, "5"}});

  EXPECT_EQ("en-US,ja,en-CA,fr-CA,zh-CN",
            language::GetLanguagesWithMaxCount(
                "en-US,ja,en-CA,fr-CA,zh-CN,en-GB,es,ru,pt,it,ko"));
}

TEST_F(LanguagePrefsTest, GetLanguageWithInvalidLimits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kReduceAcceptLanguageCount, {{kMaxAcceptLanguage.name, "0"}});

  EXPECT_EQ("en-US,ja,en-CA,fr-CA,zh-CN,en-GB,es,ru,pt,it,ko",
            language::GetLanguagesWithMaxCount(
                "en-US,ja,en-CA,fr-CA,zh-CN,en-GB,es,ru,pt,it,ko"));
}

}  // namespace language
