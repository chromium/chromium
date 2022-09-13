// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/fluent_language_model.h"

#include <cmath>

#include "base/i18n/rtl.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

using testing::ElementsAre;
using Ld = LanguageModel::LanguageDetails;

constexpr static float kFloatEps = 0.00001f;

struct PrefRegistration {
  explicit PrefRegistration(user_prefs::PrefRegistrySyncable* registry) {
    language::LanguagePrefs::RegisterProfilePrefs(registry);
    translate::TranslatePrefs::RegisterProfilePrefs(registry);
  }
};

class FluentLanguageModelTest : public testing::Test {
 protected:
  FluentLanguageModelTest()
      : prefs_(new sync_preferences::TestingPrefServiceSyncable()),
        prefs_registration_(prefs_->registry()) {}

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  PrefRegistration prefs_registration_;
};

// Compares LanguageDetails.
MATCHER_P(EqualsLd, lang_details, "") {
  return arg.lang_code == lang_details.lang_code &&
         std::abs(arg.score - lang_details.score) < kFloatEps;
}

TEST_F(FluentLanguageModelTest, Defaults) {
  // By default, languages that are blocked from translation should match the
  // list of fluent languages. Note that when using default prefs, only the UI
  // language is returned as a blocked language.
  std::string default_locale_code = base::i18n::GetConfiguredLocale();
  // Blocked languages are stored in Translate format so some languages need to
  // be converted.
  ToTranslateLanguageSynonym(&default_locale_code);

  FluentLanguageModel model(prefs_.get());
  std::vector<Ld> languages = model.GetLanguages();

  EXPECT_EQ(size_t(1), languages.size());
  EXPECT_THAT(languages[0], EqualsLd(Ld(default_locale_code, 1.0)));
}

TEST_F(FluentLanguageModelTest, ThreeBlockedLanguages) {
  base::Value::List fluent_languages;
  fluent_languages.Append("fr");
  fluent_languages.Append("ja");
  fluent_languages.Append("en");
  prefs_->SetList(translate::prefs::kBlockedLanguages,
                  std::move(fluent_languages));

  FluentLanguageModel model(prefs_.get());
  EXPECT_THAT(model.GetLanguages(), ElementsAre(EqualsLd(Ld("fr", 1.0f / 1)),
                                                EqualsLd(Ld("ja", 1.0f / 2)),
                                                EqualsLd(Ld("en", 1.0f / 3))));
}

}  // namespace language
