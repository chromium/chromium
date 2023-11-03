// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_ui_languages_manager.h"

#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

using testing::MockTranslateClient;
using testing::MockTranslateDriver;
using testing::MockTranslateRanker;
using ::testing::Test;

class MockLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class TranslateUILanguagesManagerTest : public ::testing::Test {
 public:
  TranslateUILanguagesManagerTest() = default;

  TranslateUILanguagesManagerTest(const TranslateUILanguagesManagerTest&) =
      delete;
  TranslateUILanguagesManagerTest& operator=(
      const TranslateUILanguagesManagerTest&) = delete;

  void SetUp() override {
    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);
    TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());

    client_ =
        std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
    ranker_ = std::make_unique<MockTranslateRanker>();
    language_model_ = std::make_unique<MockLanguageModel>();
    manager_ = std::make_unique<TranslateManager>(client_.get(), ranker_.get(),
                                                  language_model_.get());
    manager_->GetLanguageState()->set_translation_declined(false);

    std::vector<std::string> languages = {"ar", "de", "es", "fr"};

    languages_manager_ = std::make_unique<TranslateUILanguagesManager>(
        manager_->GetWeakPtr(), languages, "ar", "fr");
  }

  // Do not reorder. These are ordered for dependency on creation/destruction.
  MockTranslateDriver driver_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<MockTranslateClient> client_;
  std::unique_ptr<MockTranslateRanker> ranker_;
  std::unique_ptr<MockLanguageModel> language_model_;
  std::unique_ptr<TranslateManager> manager_;
  std::unique_ptr<TranslateUILanguagesManager> languages_manager_;
};

TEST_F(TranslateUILanguagesManagerTest, LanguageCodes) {
  // Test language codes.
  EXPECT_EQ("ar", languages_manager_->GetSourceLanguageCode());
  EXPECT_EQ("fr", languages_manager_->GetTargetLanguageCode());

  // Test language indices.
  const size_t ar_index = languages_manager_->GetSourceLanguageIndex();
  EXPECT_EQ("ar", languages_manager_->GetLanguageCodeAt(ar_index));
  const size_t fr_index = languages_manager_->GetTargetLanguageIndex();
  EXPECT_EQ("fr", languages_manager_->GetLanguageCodeAt(fr_index));

  // In a test environment without a set locale, language names are identical to
  // their language codes. Just test the error case.
  EXPECT_EQ(std::u16string(), languages_manager_->GetLanguageNameAt(
                                  TranslateUILanguagesManager::kNoIndex));

  // Test updating source / target codes.
  languages_manager_->UpdateSourceLanguage("es");
  EXPECT_EQ("es", languages_manager_->GetSourceLanguageCode());
  languages_manager_->UpdateTargetLanguage("de");
  EXPECT_EQ("de", languages_manager_->GetTargetLanguageCode());

  // Test updating source / target indices. Note that this also returns
  // the delegate to the starting state.
  languages_manager_->UpdateSourceLanguageIndex(ar_index);
  EXPECT_EQ("ar", languages_manager_->GetSourceLanguageCode());
  languages_manager_->UpdateTargetLanguageIndex(fr_index);
  EXPECT_EQ("fr", languages_manager_->GetTargetLanguageCode());
}

}  // namespace

}  // namespace translate
