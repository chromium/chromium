// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {

static void ExpectEqualLanguageLists(
    const base::Value* language_values,
    const std::vector<std::string>& languages) {
  const int input_size = languages.size();
  ASSERT_EQ(input_size, static_cast<int>(language_values->GetList().size()));
  for (int i = 0; i < input_size; ++i) {
    std::string value = language_values->GetList()[i].GetString();
    EXPECT_EQ(languages[i], value);
  }
}

class LanguagePrefsTest : public testing::Test {
 protected:
  LanguagePrefsTest()
      : prefs_(new sync_preferences::TestingPrefServiceSyncable()) {
    LanguagePrefs::RegisterProfilePrefs(prefs_->registry());
    language_prefs_.reset(new language::LanguagePrefs(prefs_.get()));
  }

  void ExpectFluentLanguageListContent(
      const std::vector<std::string>& list) const {
    const base::Value* values = prefs_->Get(language::prefs::kFluentLanguages);
    DCHECK(values);
    ExpectEqualLanguageLists(values, list);
  }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs_;
  std::unique_ptr<language::LanguagePrefs> language_prefs_;
};

TEST_F(LanguagePrefsTest, SetFluentTest) {
  // `en` is a default fluent language, it should be present already.
  ExpectFluentLanguageListContent({"en"});

  // One language.
  language_prefs_->SetFluent("fr-CA");
  ExpectFluentLanguageListContent({"en", "fr"});

  // Add a few more.
  language_prefs_->SetFluent("es-AR");
  language_prefs_->SetFluent("de-de");
  ExpectFluentLanguageListContent({"en", "fr", "es", "de"});

  // Add a duplicate.
  language_prefs_->ResetFluentLanguagesToDefaults();
  ExpectFluentLanguageListContent({"en"});
  language_prefs_->SetFluent("es-AR");
  language_prefs_->SetFluent("es-AR");
  ExpectFluentLanguageListContent({"en", "es"});

  // Two languages with the same base.
  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("fr-CA");
  language_prefs_->SetFluent("fr-FR");
  ExpectFluentLanguageListContent({"en", "fr"});

  // Chinese is a special case.
  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("zh-MO");
  language_prefs_->SetFluent("zh-CN");
  ExpectFluentLanguageListContent({"en", "zh-TW", "zh-CN"});

  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("zh-TW");
  language_prefs_->SetFluent("zh-HK");
  ExpectFluentLanguageListContent({"en", "zh-TW"});
}

TEST_F(LanguagePrefsTest, ClearFluentTest) {
  // Language in the list.
  // Should not clear fluent for last language.
  language_prefs_->ClearFluent("en-UK");
  ExpectFluentLanguageListContent({"en"});

  // Language in the list but with different region.
  // Should not clear fluent for last language.
  language_prefs_->ClearFluent("en-AU");
  ExpectFluentLanguageListContent({"en"});

  // Language in the list.
  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("fr");
  language_prefs_->ClearFluent("en-UK");
  ExpectFluentLanguageListContent({"fr"});

  // Language in the list but with different region.
  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("fr");
  language_prefs_->ClearFluent("en-AU");
  ExpectFluentLanguageListContent({"fr"});

  // Multiple languages.
  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("fr-CA");
  language_prefs_->SetFluent("fr-FR");
  language_prefs_->SetFluent("es-AR");
  language_prefs_->ClearFluent("fr-FR");
  ExpectFluentLanguageListContent({"en", "es"});

  // Chinese is a special case.
  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("zh-MO");
  language_prefs_->SetFluent("zh-CN");
  language_prefs_->ClearFluent("zh-TW");
  ExpectFluentLanguageListContent({"en", "zh-CN"});

  language_prefs_->ResetFluentLanguagesToDefaults();
  language_prefs_->SetFluent("zh-MO");
  language_prefs_->SetFluent("zh-CN");
  language_prefs_->ClearFluent("zh-CN");
  ExpectFluentLanguageListContent({"en", "zh-TW"});
}

TEST_F(LanguagePrefsTest, ResetEmptyFluentLanguagesToDefaultTest) {
  ExpectFluentLanguageListContent({"en"});

  language_prefs_->ResetEmptyFluentLanguagesToDefault();
  ExpectFluentLanguageListContent({"en"});

  language_prefs_->SetFluent("fr");
  language_prefs_->ResetEmptyFluentLanguagesToDefault();
  ExpectFluentLanguageListContent({"en", "fr"});

  prefs_->Set(language::prefs::kFluentLanguages, base::ListValue());
  ExpectFluentLanguageListContent({});
  language_prefs_->ResetEmptyFluentLanguagesToDefault();
  ExpectFluentLanguageListContent({"en"});
}

}  // namespace language
