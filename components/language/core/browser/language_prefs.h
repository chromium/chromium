// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/language_tag.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace base {
class Value;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;

namespace language {

inline constexpr char kFallbackInputMethodLocale[] = "en-US";

class LanguagePrefs {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit LanguagePrefs(PrefService* user_prefs);

  LanguagePrefs(const LanguagePrefs&) = delete;
  LanguagePrefs& operator=(const LanguagePrefs&) = delete;

  ~LanguagePrefs();

  // Gets the language settings list containing combination of policy-forced and
  // user-selected languages. Invalid language tags are skipped.
  std::vector<base::i18n::LanguageTag> GetAcceptLanguagesList() const;
  // Gets the user-selected language settings list. Invalid language tags are
  // skipped.
  std::vector<base::i18n::LanguageTag> GetUserSelectedLanguagesList() const;
  // Updates the user-selected language settings list. Languages that can't be
  // used as accept languages are dropped.
  void SetUserSelectedLanguagesList(
      base::span<const base::i18n::LanguageTag> languages);
  // Returns true if the target language is forced through policy.
  bool IsForcedLanguage(const base::i18n::LanguageTag& language) const;

#if BUILDFLAG(IS_ANDROID)
  // Get the ULP languages from a preference. This is an unfiltered list of
  // languages and may contain country specific language locales. If you do not
  // need specific locals always compare base languages from the list.
  std::vector<base::i18n::LanguageTag> GetULPLanguages() const;
  // Clear the previous ULP language pref and set to the new list of languages.
  void SetULPLanguages(std::vector<base::i18n::LanguageTag> ulp_languages);
#endif

 private:
  // Returns the language list containing combination of policy-forced and
  // user-selected languages, and updates `forced_languages_set_`.
  std::vector<base::i18n::LanguageTag> GetDeduplicatedUserLanguages();
  // Updates the pref corresponding to the language list containing combination
  // of policy-forced and user-selected languages.
  // Since languages may be removed from the policy while the browser is off,
  // having an additional policy solely for user-selected languages allows
  // Chrome to clear any removed policy languages from the accept languages pref
  // while retaining all user-selected languages.
  void UpdateAcceptLanguagesPref();
  // Initializes the user selected language pref to ensure backwards
  // compatibility.
  void InitializeSelectedLanguagesPref();

  // Used for deduplication and reordering of languages.
  base::flat_set<base::i18n::LanguageTag> forced_languages_set_;

  raw_ptr<PrefService> prefs_;  // Weak.
  PrefChangeRegistrar pref_change_registrar_;
};

void ResetLanguagePrefs(PrefService* prefs);

// Given a comma separated list of locales, return the first.
std::string GetFirstLanguage(std::string_view language_list);

// Given a comma separated list of locales, return the top number of languages
// based on the setting.
std::string GetLanguagesWithMaxCount(std::string_view language_list);

// Given a comma separated list of language tags, return a new list which keeps
// the same first language tag but otherwise matches one of Chrome's standard
// default configurations. This is to reduce the identifiability when incognito
// of users who've configured unusual language preferences.
std::string GetIncognitoLanguageList(std::string_view language_list);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
