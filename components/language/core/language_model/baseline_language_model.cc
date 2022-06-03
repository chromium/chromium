// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/language_model/baseline_language_model.h"

#include <unordered_set>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "components/prefs/pref_service.h"

namespace language {

namespace {
constexpr float kUrlLanguageFreqCutoff = 0.3f;
}  // namespace

BaselineLanguageModel::BaselineLanguageModel(
    PrefService* const pref_service,
    const std::string& ui_lang,
    const std::string& accept_langs_pref)
    : pref_service_(pref_service),
      ui_lang_(ui_lang),
      accept_langs_pref_(accept_langs_pref),
      lang_histogram_(pref_service) {
  DCHECK(pref_service);
  DCHECK(!ui_lang.empty());
  DCHECK(!accept_langs_pref.empty());
  DCHECK(pref_service->FindPreference(accept_langs_pref));
}

std::vector<LanguageModel::LanguageDetails>
BaselineLanguageModel::GetLanguages() {
  // Start with UI language.
  std::vector<LanguageDetails> lang_details = {LanguageDetails(ui_lang_, 1.0f)};
  std::unordered_set<std::string> seen = {ui_lang_};

  // Then add sufficiently-frequent URL languages.
  const std::vector<UrlLanguageHistogram::LanguageInfo> hist_langs =
      lang_histogram_.GetTopLanguages();
  for (const UrlLanguageHistogram::LanguageInfo& lang_info : hist_langs) {
    if (lang_info.frequency < kUrlLanguageFreqCutoff)
      break;

    if (seen.find(lang_info.language_code) != seen.end())
      continue;

    lang_details.push_back(LanguageDetails(lang_info.language_code,
                                           1.0f / (lang_details.size() + 1)));
    seen.insert(lang_info.language_code);
  }

  // Then add accept languages.
  const std::vector<std::string> accept_langs =
      base::SplitString(pref_service_->GetString(accept_langs_pref_), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const std::string& lang_code : accept_langs) {
    if (seen.find(lang_code) != seen.end())
      continue;

    lang_details.push_back(
        LanguageDetails(lang_code, 1.0f / (lang_details.size() + 1)));
    seen.insert(lang_code);
  }

  return lang_details;
}

}  // namespace language
