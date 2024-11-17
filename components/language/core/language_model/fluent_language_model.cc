// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/fluent_language_model.h"

#include "base/strings/string_split.h"
#include "components/prefs/pref_service.h"

#include "components/translate/core/browser/translate_prefs.h"

namespace language {

FluentLanguageModel::FluentLanguageModel(PrefService* const pref_service)
    : translate_prefs_(
          std::make_unique<translate::TranslatePrefs>(pref_service)) {
  DCHECK(pref_service);
}

FluentLanguageModel::~FluentLanguageModel() = default;

std::vector<LanguageModel::LanguageDetails>
FluentLanguageModel::GetLanguages() {
  std::vector<LanguageDetails> lang_details;
  // Languages that are blocked from translation are assumed to be languages
  // that the user is fluent in.
  for (const std::string& lang_code :
       translate_prefs_->GetNeverTranslateLanguages()) {
    lang_details.emplace_back(
        LanguageDetails(lang_code, 1.0f / (lang_details.size() + 1)));
  }

  return lang_details;
}

}  // namespace language
