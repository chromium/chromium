// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/accept_languages_service.h"

#include <stddef.h>

#include <string_view>

#include "base/functional/bind.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/i18n/rtl.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/chromium_language_matcher.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagConverter;
using ::base::i18n::LanguageTagMatcher;

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
  std::optional<LanguageTag> parsed_tag =
      LanguageTagConverter::GetInstance().FromString(language);
  if (!parsed_tag) {
    return false;
  }

  const std::string ui_locale = base::i18n::GetConfiguredLocale();

  return l10n_util::IsAcceptLanguageDisplayable(ui_locale,
                                                parsed_tag->tag_string());
}

bool AcceptLanguagesService::IsAcceptLanguage(std::string_view language) const {
  if (!accept_languages_matcher_) {
    return false;
  }

  std::optional<LanguageTag> parsed_tag =
      LanguageTagConverter::GetInstance().FromString(language);
  if (!parsed_tag) {
    return false;
  }
  std::optional<LanguageTag> language_tag_match =
      accept_languages_matcher_->Match(*parsed_tag);
  // The regions are compared to avoid accepting "zh" when there is only "zh-CN"
  // or "zh-TW" available.
  return language_tag_match.has_value() &&
         (language_tag_match->region_subtag().empty() ||
          language_tag_match->region_subtag() == parsed_tag->region_subtag());
}

void AcceptLanguagesService::InitAcceptLanguages(PrefService* prefs) {
  DCHECK(prefs);
  // Build the languages.
  std::string accept_languages_pref = prefs->GetString(accept_languages_pref_);
  std::vector<LanguageTag> accept_languages;

  for (std::string_view lang :
       base::SplitStringPiece(accept_languages_pref, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    std::optional<LanguageTag> language_tag =
        LanguageTagConverter::GetInstance().FromString(lang);
    if (!language_tag) {
      continue;
    }

    if (language_tag->region_subtag().empty() ||
        language_tag == GetKnownLanguageTag("zh-TW") ||
        language_tag == GetKnownLanguageTag("zh-CN")) {
      accept_languages.push_back(*language_tag);
      continue;
    }

    // Get rid of the locale extension if any (ex: en-US -> en), but for Chinese
    // for which the CLD reports zh-CN and zh-TW.
    accept_languages.push_back(language_tag->WithLanguageSubtagOnly());
  }

  accept_languages_matcher_ = std::make_unique<LanguageTagMatcher>(
      LanguageTagMatcher::Create(std::move(accept_languages)));
}

}  // namespace language
