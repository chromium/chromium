// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_

#include <string>

#include "base/macros.h"

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

  LanguagePrefs(PrefService* user_prefs);

  // Return true iff the user is fluent in the given |language|.
  bool IsFluent(const std::string& language) const;
  // Mark that the user is fluent in the given |language|.
  void SetFluent(const std::string& language);
  // Remove the given |language| from the user's fluent languages.
  void ClearFluent(const std::string& language);
  // Reset the fluent languages to their defaults.
  void ResetFluentLanguagesToDefaults();
  // Get the default fluent languages for the user.
  static base::Value GetDefaultFluentLanguages();
  // If the list of fluent languages is empty, reset it to defaults.
  void ResetEmptyFluentLanguagesToDefault();

 private:
  base::Value* GetFluentLanguages();

  const base::Value* GetFluentLanguages() const;

  size_t NumFluentLanguages() const;

  PrefService* prefs_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(LanguagePrefs);
};

void ResetLanguagePrefs(PrefService* prefs);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_H_
