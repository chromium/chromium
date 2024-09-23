// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/accept_languages_service.h"

#include <stddef.h>

#include <string_view>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

AcceptLanguagesService::AcceptLanguagesService(
    PrefService* prefs,
    const char* accept_languages_pref)
    : accept_languages_pref_(accept_languages_pref) {
  InitAcceptLanguages(prefs);

  // Also start listening for changes in the accept languages.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      accept_languages_pref,
      base::BindRepeating(&AcceptLanguagesService::InitAcceptLanguages,
                          base::Unretained(this), prefs));
}

AcceptLanguagesService::~AcceptLanguagesService() = default;

// static
bool AcceptLanguagesService::CanBeAcceptLanguage(std::string_view language) {
  std::string accept_language(language);
  language::ToChromeLanguageSynonym(&accept_language);

  const std::string ui_locale = base::i18n::GetConfiguredLocale();

  return l10n_util::IsAcceptLanguageDisplayable(ui_locale, accept_language);
}

bool AcceptLanguagesService::IsAcceptLanguage(std::string_view language) const {
  std::string accept_language(language);
  language::ToChromeLanguageSynonym(&accept_language);
  return accept_languages_.find(accept_language) != accept_languages_.end();
}

void AcceptLanguagesService::InitAcceptLanguages(PrefService* prefs) {
  DCHECK(prefs);
  // Build the languages.
  accept_languages_.clear();
  std::string accept_languages_pref = prefs->GetString(accept_languages_pref_);
  for (std::string_view lang :
       base::SplitStringPiece(accept_languages_pref, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    size_t index = lang.find('-');
    if (index != std::string_view::npos && lang != "zh-CN" && lang != "zh-TW") {
      accept_languages_.insert(std::string(lang.substr(0, index)));
    }
    accept_languages_.insert(std::string(lang));
  }
}

}  // namespace language
