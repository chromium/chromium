// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/mock_translate_infobar_delegate.h"

namespace translate {

namespace testing {

std::vector<MockLanguageModel::LanguageDetails>
MockLanguageModel::GetLanguages() {
  return {MockLanguageModel::LanguageDetails("en", 1.0)};
}

void MockTranslateInfoBarDelegate::SetTranslateLanguagesForTest(
    std::vector<std::pair<std::string, std::u16string>> languages) {
  for (auto pair : languages) {
    languages_.push_back(pair);
  }
}

void MockTranslateInfoBarDelegate::SetContentLanguagesCodesForTest(
    std::vector<std::string> languages) {
  for (auto& entry : languages) {
    content_languages_.push_back(entry);
  }
}

MockTranslateInfoBarDelegate::MockTranslateInfoBarDelegate(
    const base::WeakPtr<translate::TranslateManager>& translate_manager,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool triggered_from_menu)
    : translate::TranslateInfoBarDelegate(translate_manager,
                                          step,
                                          source_language,
                                          target_language,
                                          error_type,
                                          triggered_from_menu) {}

MockTranslateInfoBarDelegate::~MockTranslateInfoBarDelegate() {}

MockTranslateInfoBarDelegateFactory::MockTranslateInfoBarDelegateFactory(
    const std::string& source_language,
    const std::string& target_language) {
  pref_service_ =
      std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
  language::LanguagePrefs::RegisterProfilePrefs(pref_service_->registry());
  translate::TranslatePrefs::RegisterProfilePrefs(pref_service_->registry());
  pref_service_->registry()->RegisterBooleanPref(prefs::kOfferTranslateEnabled,
                                                 true);
  client_ =
      std::make_unique<MockTranslateClient>(&driver_, pref_service_.get());
  ranker_ = std::make_unique<MockTranslateRanker>();
  language_model_ = std::make_unique<MockLanguageModel>();
  manager_ = std::make_unique<translate::TranslateManager>(
      client_.get(), ranker_.get(), language_model_.get());
  delegate_ = std::make_unique<MockTranslateInfoBarDelegate>(
      manager_->GetWeakPtr(),
      translate::TranslateStep::TRANSLATE_STEP_BEFORE_TRANSLATE,
      source_language, target_language, translate::TranslateErrors::NONE,
      false);
}

MockTranslateInfoBarDelegateFactory::~MockTranslateInfoBarDelegateFactory() {}

// static
std::unique_ptr<MockTranslateInfoBarDelegate>
MockTranslateInfoBarDelegateFactory::CreateMockTranslateInfoBarDelegate(
    translate::TranslateStep step) {
  return std::make_unique<MockTranslateInfoBarDelegate>(
      manager_->GetWeakPtr(), step, "fr", "en",
      translate::TranslateErrors::NONE, false);
}

}  // namespace testing

}  // namespace translate
