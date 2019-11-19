// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/fluent_language_model.h"

#include "base/strings/string_split.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/prefs/pref_service.h"

namespace language {

FluentLanguageModel::FluentLanguageModel(PrefService* const pref_service,
                                         const std::string& accept_langs_pref)
    : pref_service_(pref_service),
      accept_langs_pref_(accept_langs_pref),
      language_prefs_(std::make_unique<LanguagePrefs>(pref_service)) {
  DCHECK(pref_service);
  DCHECK(!accept_langs_pref.empty());
  DCHECK(pref_service->FindPreference(accept_langs_pref));
}

FluentLanguageModel::~FluentLanguageModel() {}

std::vector<LanguageModel::LanguageDetails>
FluentLanguageModel::GetLanguages() {
  std::vector<LanguageDetails> lang_details;
  const std::vector<std::string> accept_langs =
      base::SplitString(pref_service_->GetString(accept_langs_pref_), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& lang_code : accept_langs) {
    if (language_prefs_->IsFluent(lang_code)) {
      lang_details.push_back(
          LanguageDetails(lang_code, 1.0f / (lang_details.size() + 1)));
    }
  }
  return lang_details;
}

}  // namespace language
