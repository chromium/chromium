// Copyright 2020 The Chromium Authors. All rights reserved.
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

AcceptLanguagesTester::AcceptLanguagesTester(PrefService* user_prefs)
    : prefs_(user_prefs) {}

void AcceptLanguagesTester::ExpectLanguagePrefs(
    const std::string& expected_prefs,
    const std::string& expected_prefs_chromeos) const {
  if (expected_prefs.empty()) {
    EXPECT_TRUE(prefs_->GetString(language::prefs::kAcceptLanguages).empty());
  } else {
    EXPECT_EQ(expected_prefs,
              prefs_->GetString(language::prefs::kAcceptLanguages));
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (expected_prefs_chromeos.empty()) {
    EXPECT_TRUE(
        prefs_->GetString(language::prefs::kPreferredLanguages).empty());
  } else {
    EXPECT_EQ(expected_prefs_chromeos,
              prefs_->GetString(language::prefs::kPreferredLanguages));
  }
#endif
}

// Similar to function above: this one expects both ChromeOS and other
// platforms to have the same value of language prefs.
void AcceptLanguagesTester::ExpectLanguagePrefs(
    const std::string& expected_prefs) const {
  ExpectLanguagePrefs(expected_prefs, expected_prefs);
}

void AcceptLanguagesTester::SetLanguagePrefs(
    const std::vector<std::string>& languages) {
  std::string languages_str = base::JoinString(languages, ",");
#if BUILDFLAG(IS_CHROMEOS_ASH)
  prefs_->SetString(language::prefs::kPreferredLanguages, languages_str);
#endif

  prefs_->SetString(language::prefs::kAcceptLanguages, languages_str);
}

}  // namespace test
}  // namespace language
