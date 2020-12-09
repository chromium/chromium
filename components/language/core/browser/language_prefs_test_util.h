// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_TEST_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_TEST_UTIL_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"

class PrefService;

namespace language {
namespace test {

// Helper class for testing Accept Languages.
class AcceptLanguagesTester {
 public:
  explicit AcceptLanguagesTester(PrefService* user_prefs);

  // Checks that the provided strings are equivalent to the content language
  // prefs. Chrome OS uses a different pref, so we need to handle it separately.
  void ExpectLanguagePrefs(const std::string& expected_prefs,
                           const std::string& expected_prefs_chromeos) const;

  // Similar to function above: this one expects both ChromeOS and other
  // platforms to have the same value of language prefs.
  void ExpectLanguagePrefs(const std::string& expected_prefs) const;

  void SetLanguagePrefs(const std::vector<std::string>& languages);

 private:
  PrefService* prefs_;
};

}  // namespace test
}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_BROWSER_LANGUAGE_PREFS_TEST_UTIL_H_
