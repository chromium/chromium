// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace language {

const char kFallbackInputMethodLocale[] = "en-US";

void LanguagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(language::prefs::kAcceptLanguages,
                               l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterStringPref(language::prefs::kSelectedLanguages,
                               std::string(),
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterListPref(language::prefs::kForcedLanguages);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterStringPref(language::prefs::kPreferredLanguages,
                               kFallbackInputMethodLocale);

  registry->RegisterStringPref(
      language::prefs::kPreferredLanguagesSyncable, "",
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
#endif
  registry->RegisterListPref(language::prefs::kFluentLanguages,
                             LanguagePrefs::GetDefaultFluentLanguages(),
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

LanguagePrefs::LanguagePrefs(PrefService* user_prefs) : prefs_(user_prefs) {
  ResetEmptyFluentLanguagesToDefault();
  InitializeSelectedLanguagesPref();
  UpdateAcceptLanguagesPref();
  base::RepeatingClosure callback = base::BindRepeating(
      &LanguagePrefs::UpdateAcceptLanguagesPref, base::Unretained(this));
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(language::prefs::kForcedLanguages, callback);
  pref_change_registrar_.Add(language::prefs::kSelectedLanguages, callback);
}

LanguagePrefs::~LanguagePrefs() {
  pref_change_registrar_.RemoveAll();
}

bool LanguagePrefs::IsFluent(base::StringPiece language) const {
  std::string canonical_lang(language);
  language::ToTranslateLanguageSynonym(&canonical_lang);
  const base::Value* fluents =
      prefs_->GetList(language::prefs::kFluentLanguages);
  return base::Contains(fluents->GetList(),
                        base::Value(std::move(canonical_lang)));
}

void LanguagePrefs::SetFluent(base::StringPiece language) {
  if (IsFluent(language))
    return;
  std::string canonical_lang(language);
  language::ToTranslateLanguageSynonym(&canonical_lang);
  ListPrefUpdate update(prefs_, language::prefs::kFluentLanguages);
  update->Append(std::move(canonical_lang));
}

void LanguagePrefs::ClearFluent(base::StringPiece language) {
  if (NumFluentLanguages() <= 1)  // Never remove last fluent language.
    return;
  std::string canonical_lang(language);
  language::ToTranslateLanguageSynonym(&canonical_lang);
  ListPrefUpdate update(prefs_, language::prefs::kFluentLanguages);
  update->EraseListValue(base::Value(std::move(canonical_lang)));
}

void LanguagePrefs::ResetFluentLanguagesToDefaults() {
  // Reset pref to defaults.
  prefs_->ClearPref(language::prefs::kFluentLanguages);
}

std::vector<std::string> LanguagePrefs::GetFluentLanguages() const {
  const base::Value* fluent_languages_value =
      prefs_->GetList(language::prefs::kFluentLanguages);
  if (!fluent_languages_value) {
    NOTREACHED() << "Fluent languages pref is unregistered";
  }

  std::vector<std::string> languages;
  for (const auto& language : fluent_languages_value->GetList()) {
    std::string chrome_language(language.GetString());
    language::ToChromeLanguageSynonym(&chrome_language);
    languages.push_back(chrome_language);
  }
  return languages;
}

void LanguagePrefs::ResetEmptyFluentLanguagesToDefault() {
  if (NumFluentLanguages() == 0)
    ResetFluentLanguagesToDefaults();
}

void LanguagePrefs::GetAcceptLanguagesList(
    std::vector<std::string>* languages) const {
  DCHECK(languages);
  DCHECK(languages->empty());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const std::string& key = language::prefs::kPreferredLanguages;
#else
  const std::string& key = language::prefs::kAcceptLanguages;
#endif

  *languages = base::SplitString(prefs_->GetString(key), ",",
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void LanguagePrefs::GetUserSelectedLanguagesList(
    std::vector<std::string>* languages) const {
  DCHECK(languages);
  DCHECK(languages->empty());
  const std::string& key = language::prefs::kSelectedLanguages;
  *languages = base::SplitString(prefs_->GetString(key), ",",
                                 base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
}

void LanguagePrefs::SetUserSelectedLanguagesList(
    const std::vector<std::string>& languages) {
  std::string languages_str = base::JoinString(languages, ",");
  prefs_->SetString(language::prefs::kSelectedLanguages, languages_str);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  prefs_->SetString(language::prefs::kPreferredLanguages, languages_str);
#endif
}

void LanguagePrefs::GetDeduplicatedUserLanguages(
    std::string* deduplicated_languages_string) {
  std::vector<std::string> deduplicated_languages;
  forced_languages_set_.clear();

  // Add policy languages.
  for (const auto& language :
       *prefs_->GetList(language::prefs::kForcedLanguages)) {
    if (forced_languages_set_.find(language.GetString()) ==
        forced_languages_set_.end()) {
      deduplicated_languages.emplace_back(language.GetString());
      forced_languages_set_.insert(language.GetString());
    }
  }

  // Add non-duplicate user-selected languages.
  for (auto& language :
       base::SplitString(prefs_->GetString(language::prefs::kSelectedLanguages),
                         ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (forced_languages_set_.find(language) == forced_languages_set_.end())
      deduplicated_languages.emplace_back(std::move(language));
  }
  *deduplicated_languages_string =
      base::JoinString(deduplicated_languages, ",");
}

void LanguagePrefs::UpdateAcceptLanguagesPref() {
  std::string deduplicated_languages_string;
  GetDeduplicatedUserLanguages(&deduplicated_languages_string);
  if (deduplicated_languages_string !=
      prefs_->GetString(language::prefs::kAcceptLanguages))
    prefs_->SetString(language::prefs::kAcceptLanguages,
                      deduplicated_languages_string);
}

bool LanguagePrefs::IsForcedLanguage(const std::string& language) {
  return forced_languages_set_.find(language) != forced_languages_set_.end();
}

void LanguagePrefs::InitializeSelectedLanguagesPref() {
  // Initializes user-selected languages if they're empty.
  // This is important so that previously saved languages aren't overwritten.
  if (prefs_->GetString(language::prefs::kSelectedLanguages).empty()) {
    prefs_->SetString(language::prefs::kSelectedLanguages,
                      prefs_->GetString(language::prefs::kAcceptLanguages));
  }
}

// static
base::Value LanguagePrefs::GetDefaultFluentLanguages() {
  typename base::Value::ListStorage languages;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Preferred languages.
  std::string language = language::kFallbackInputMethodLocale;
  language::ToTranslateLanguageSynonym(&language);
  languages.push_back(base::Value(std::move(language)));
#else
  // Accept languages.
#pragma GCC diagnostic push
// See comment above the |break;| in the loop just below for why.
#pragma GCC diagnostic ignored "-Wunreachable-code"
  for (std::string& language :
       base::SplitString(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    language::ToTranslateLanguageSynonym(&language);
    languages.push_back(base::Value(std::move(language)));

    // crbug.com/958348: The default value for Accept-Language *should* be the
    // same as the one for Fluent Languages. However, Accept-Language contains
    // English (and more) in addition to the local language in most locales due
    // to historical reasons. Exiting early from this loop is a temporary fix
    // that allows Fluent Languages to be at least populated with the UI
    // language while still allowing Translate to trigger on other languages,
    // most importantly English.
    // Once the change to remove English from Accept-Language defaults lands,
    // this break should be removed to enable the Fluent Language List and the
    // Accept-Language list to be initialized to the same values.
    break;
#pragma GCC diagnostic pop
  }
#endif

  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()),
                  languages.end());

  return base::Value(std::move(languages));
}

size_t LanguagePrefs::NumFluentLanguages() const {
  const base::Value* fluents =
      prefs_->GetList(language::prefs::kFluentLanguages);
  return fluents->GetList().size();
}

void ResetLanguagePrefs(PrefService* prefs) {
  prefs->ClearPref(language::prefs::kSelectedLanguages);
  prefs->ClearPref(language::prefs::kAcceptLanguages);
  prefs->ClearPref(language::prefs::kFluentLanguages);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  prefs->ClearPref(language::prefs::kPreferredLanguages);
  prefs->ClearPref(language::prefs::kPreferredLanguagesSyncable);
#endif
}

std::string GetFirstLanguage(base::StringPiece language_list) {
  auto end = language_list.find(",");
  return std::string(language_list.substr(0, end));
}

}  // namespace language
