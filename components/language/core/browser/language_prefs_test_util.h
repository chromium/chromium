// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_TEST_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_TEST_UTIL_H_

#include <string>

#include "base/memory/raw_ptr.h"

class PrefService;

namespace language {
namespace test {

// Helper class for testing Accept Languages.
class LanguagePrefTester {
 public:
  explicit LanguagePrefTester(PrefService* user_prefs);

  // Checks that the provided strings are equivalent to the language pref of
  // interest.
  void ExpectPref(const std::string& pref_name,
                  const std::string& expected_prefs,
                  const std::string& expected_prefs_chromeos) const;

  // Checks that the provided strings are equivalent to the accept languages
  // pref. Chrome OS uses a different pref, so we need to handle it separately.
  void ExpectAcceptLanguagePrefs(
      const std::string& expected_prefs,
      const std::string& expected_prefs_chromeos) const;

  // Similar to function above: this one expects both ChromeOS and other
  // platforms to have the same value of language prefs.
  void ExpectAcceptLanguagePrefs(const std::string& expected_prefs) const;

  // Checks that the provided strings are equivalent to the selected languages
  // pref.
  void ExpectSelectedLanguagePrefs(
      const std::string& expected_prefs,
      const std::string& expected_prefs_chromeos) const;

  // Similar to function above: this one expects both ChromeOS and other
  // platforms to have the same value of language prefs.
  void ExpectSelectedLanguagePrefs(const std::string& expected_prefs) const;

  // Sets the contents of the selected language pref. Chrome OS uses a different
  // pref so it is handled separately.
  void SetLanguagePrefs(const std::vector<std::string>& languages);

  // Sets the contents of the forced language pref.
  void SetForcedLanguagePrefs(std::vector<std::string>&& languages);

 private:
  raw_ptr<PrefService> prefs_;
};

}  // namespace test
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_TEST_UTIL_H_
