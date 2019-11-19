// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_accept_languages.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace translate {

TranslateAcceptLanguages::TranslateAcceptLanguages(
    PrefService* prefs,
    const char* accept_languages_pref)
    : accept_languages_pref_(accept_languages_pref) {
  InitAcceptLanguages(prefs);

  // Also start listening for changes in the accept languages.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      accept_languages_pref,
      base::Bind(&TranslateAcceptLanguages::InitAcceptLanguages,
                 base::Unretained(this),
                 prefs));
}

TranslateAcceptLanguages::~TranslateAcceptLanguages() {
}

// static
bool TranslateAcceptLanguages::CanBeAcceptLanguage(
    const std::string& language) {
  SCOPED_UMA_HISTOGRAM_TIMER("Translate.AcceptLanguages.CanBeAcceptDuration");

  std::string accept_language = language;
  language::ToChromeLanguageSynonym(&accept_language);

  const std::string locale =
      TranslateDownloadManager::GetInstance()->application_locale();

  return l10n_util::IsLanguageAccepted(locale, accept_language);
}

bool TranslateAcceptLanguages::IsAcceptLanguage(const std::string& language) {
  std::string accept_language = language;
  language::ToChromeLanguageSynonym(&accept_language);
  return accept_languages_.find(accept_language) != accept_languages_.end();
}

void TranslateAcceptLanguages::InitAcceptLanguages(PrefService* prefs) {
  DCHECK(prefs);
  // Build the languages.
  accept_languages_.clear();
  std::string accept_languages_pref = prefs->GetString(accept_languages_pref_);
  for (const base::StringPiece& lang : base::SplitStringPiece(
           accept_languages_pref, ",",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    size_t index = lang.find('-');
    if (index != base::StringPiece::npos && lang != "zh-CN" && lang != "zh-TW")
      accept_languages_.insert(lang.substr(0, index).as_string());
    accept_languages_.insert(lang.as_string());
  }
}

}  // namespace translate
