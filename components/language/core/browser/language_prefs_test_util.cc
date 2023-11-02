// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language/core/browser/language_prefs_test_util.h"

#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace language {
namespace test {

LanguagePrefTester::LanguagePrefTester(PrefService* user_prefs)
    : prefs_(user_prefs) {}

void LanguagePrefTester::ExpectPref(
    const std::string& pref_name,
    const std::string& expected_prefs,
    const std::string& expected_prefs_chromeos) const {
  if (expected_prefs.empty()) {
    EXPECT_TRUE(prefs_->GetString(pref_name).empty());
  } else {
    EXPECT_EQ(expected_prefs, prefs_->GetString(pref_name));
  }
}

void LanguagePrefTester::ExpectAcceptLanguagePrefs(
    const std::string& expected_prefs,
    const std::string& expected_prefs_chromeos) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ExpectPref(language::prefs::kPreferredLanguages, expected_prefs,
             expected_prefs_chromeos);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  ExpectPref(language::prefs::kAcceptLanguages, expected_prefs,
             expected_prefs_chromeos);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Similar to function above: this one expects both ChromeOS and other
// platforms to have the same value of language prefs.
void LanguagePrefTester::ExpectAcceptLanguagePrefs(
    const std::string& expected_prefs) const {
  ExpectAcceptLanguagePrefs(expected_prefs, expected_prefs);
}

void LanguagePrefTester::ExpectSelectedLanguagePrefs(
    const std::string& expected_prefs,
    const std::string& expected_prefs_chromeos) const {
  ExpectPref(language::prefs::kSelectedLanguages, expected_prefs,
             expected_prefs_chromeos);
}

// Similar to function above: this one expects both ChromeOS and other
// platforms to have the same value of language prefs.
void LanguagePrefTester::ExpectSelectedLanguagePrefs(
    const std::string& expected_prefs) const {
  ExpectSelectedLanguagePrefs(expected_prefs, expected_prefs);
}

void LanguagePrefTester::SetLanguagePrefs(
    const std::vector<std::string>& languages) {
  std::string languages_str = base::JoinString(languages, ",");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  prefs_->SetString(language::prefs::kPreferredLanguages, languages_str);
#endif
  prefs_->SetString(language::prefs::kSelectedLanguages, languages_str);
}

void LanguagePrefTester::SetForcedLanguagePrefs(
    std::vector<std::string>&& languages) {
  base::Value::List languages_list;

  for (std::string language : languages) {
    languages_list.Append(std::move(language));
  }

  prefs_->SetList(language::prefs::kForcedLanguages, std::move(languages_list));
}

}  // namespace test
}  // namespace language
