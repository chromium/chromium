// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
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
#if defined(OS_CHROMEOS)
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
}

bool LanguagePrefs::IsFluent(const std::string& language) const {
  std::string canonical_lang = language;
  language::ToTranslateLanguageSynonym(&canonical_lang);
  const base::Value* fluents =
      prefs_->GetList(language::prefs::kFluentLanguages);
  return base::Contains(fluents->GetList(), base::Value(canonical_lang));
}

void LanguagePrefs::SetFluent(const std::string& language) {
  if (IsFluent(language))
    return;
  std::string canonical_lang = language;
  language::ToTranslateLanguageSynonym(&canonical_lang);
  ListPrefUpdate update(prefs_, language::prefs::kFluentLanguages);
  update->Append(std::move(canonical_lang));
}

void LanguagePrefs::ClearFluent(const std::string& language) {
  if (NumFluentLanguages() <= 1)  // Never remove last fluent language.
    return;
  std::string canonical_lang = language;
  language::ToTranslateLanguageSynonym(&canonical_lang);
  ListPrefUpdate update(prefs_, language::prefs::kFluentLanguages);
  base::Erase(update->GetList(), base::Value(canonical_lang));
}

void LanguagePrefs::ResetFluentLanguagesToDefaults() {
  // Reset pref to defaults.
  prefs_->ClearPref(language::prefs::kFluentLanguages);
}

void LanguagePrefs::ResetEmptyFluentLanguagesToDefault() {
  if (NumFluentLanguages() == 0)
    ResetFluentLanguagesToDefaults();
}

base::Value LanguagePrefs::GetDefaultFluentLanguages() {
  std::set<std::string> languages;
#if defined(OS_CHROMEOS)
  // Preferred languages.
  std::string language = language::kFallbackInputMethodLocale;
  language::ToTranslateLanguageSynonym(&language);
  languages.insert(std::move(language));
#else
  // Accept languages.
  for (std::string language :
       base::SplitString(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES), ",",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    language::ToTranslateLanguageSynonym(&language);
    languages.insert(std::move(language));

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
  }
#endif
  base::Value language_values(base::Value::Type::LIST);
  for (const std::string& language : languages)
    language_values.Append(language);

  return language_values;
}

size_t LanguagePrefs::NumFluentLanguages() const {
  const base::Value* fluents =
      prefs_->GetList(language::prefs::kFluentLanguages);
  return fluents->GetList().size();
}

void ResetLanguagePrefs(PrefService* prefs) {
  prefs->ClearPref(language::prefs::kAcceptLanguages);
  prefs->ClearPref(language::prefs::kFluentLanguages);
#if defined(OS_CHROMEOS)
  prefs->ClearPref(language::prefs::kPreferredLanguages);
  prefs->ClearPref(language::prefs::kPreferredLanguagesSyncable);
#endif
}

}  // namespace language
