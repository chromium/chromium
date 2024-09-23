// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_download_manager.h"

#include <string_view>

#include "base/check.h"
#include "base/memory/singleton.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/translate_switches.h"

namespace translate {

// static
TranslateDownloadManager* TranslateDownloadManager::GetInstance() {
  return base::Singleton<TranslateDownloadManager>::get();
}

TranslateDownloadManager::TranslateDownloadManager()
    : language_list_(std::make_unique<TranslateLanguageList>()),
      script_(std::make_unique<TranslateScript>()) {}

TranslateDownloadManager::~TranslateDownloadManager() {}

void TranslateDownloadManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  language_list_.reset();
  script_.reset();
  url_loader_factory_ = nullptr;
}

// static
void TranslateDownloadManager::GetSupportedLanguages(
    bool translate_allowed,
    std::vector<std::string>* languages) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  DCHECK(language_list);

  language_list->GetSupportedLanguages(translate_allowed, languages);
}

// static
void TranslateDownloadManager::RequestLanguageList() {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  DCHECK(language_list);
  language_list->RequestLanguageList();
}

// static
base::Time TranslateDownloadManager::GetSupportedLanguagesLastUpdated() {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  DCHECK(language_list);

  return language_list->last_updated();
}

// static
std::string TranslateDownloadManager::GetLanguageCode(
    std::string_view language) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  DCHECK(language_list);

  return language_list->GetLanguageCode(language);
}

// static
bool TranslateDownloadManager::IsSupportedLanguage(std::string_view language) {
  TranslateLanguageList* language_list = GetInstance()->language_list();
  DCHECK(language_list);

  return language_list->IsSupportedLanguage(language);
}

void TranslateDownloadManager::ClearTranslateScriptForTesting() {
  DCHECK(script_);
  script_->Clear();
}

void TranslateDownloadManager::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  language_list_ = std::make_unique<TranslateLanguageList>();
  script_ = std::make_unique<TranslateScript>();
  url_loader_factory_ = nullptr;
}

void TranslateDownloadManager::SetTranslateScriptExpirationDelay(int delay_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(script_);
  script_->set_expiration_delay(delay_ms);
}

}  // namespace translate
