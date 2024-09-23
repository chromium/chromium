// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

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

extern const char kFallbackInputMethodLocale[];

class LanguagePrefs {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit LanguagePrefs(PrefService* user_prefs);

  LanguagePrefs(const LanguagePrefs&) = delete;
  LanguagePrefs& operator=(const LanguagePrefs&) = delete;

  ~LanguagePrefs();

  // Gets the language settings list containing combination of policy-forced and
  // user-selected languages. Language settings list follows the Chrome internal
  // format.
  void GetAcceptLanguagesList(std::vector<std::string>* languages) const;
  // Gets the user-selected language settings list. Languages are expected to be
  // in the Chrome internal format.
  void GetUserSelectedLanguagesList(std::vector<std::string>* languages) const;
  // Updates the user-selected language settings list. Languages are expected to
  // be in the Chrome internal format.
  void SetUserSelectedLanguagesList(const std::vector<std::string>& languages);
  // Returns true if the target language is forced through policy.
  bool IsForcedLanguage(const std::string& language);

#if BUILDFLAG(IS_ANDROID)
  // Get the ULP languages from a preference. This is an unfiltered list of
  // languages and may contain country specific language locales. If you do not
  // need specific locals always compare base languages from the list.
  std::vector<std::string> GetULPLanguages();
  // Clear the previous ULP language pref and set to the new list of languages.
  void SetULPLanguages(std::vector<std::string> ulp_languages);
#endif

 private:
  // Updates the language list containing combination of policy-forced and
  // user-selected languages.
  void GetDeduplicatedUserLanguages(std::string* deduplicated_languages_string);
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
  std::set<std::string> forced_languages_set_;

  raw_ptr<PrefService> prefs_;  // Weak.
  PrefChangeRegistrar pref_change_registrar_;
};

void ResetLanguagePrefs(PrefService* prefs);

// Given a comma separated list of locales, return the first.
std::string GetFirstLanguage(std::string_view language_list);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
