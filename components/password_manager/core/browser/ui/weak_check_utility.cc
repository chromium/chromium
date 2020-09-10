// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include "base/strings/utf_string_conversions.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/matching.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/scoring.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/time_estimates.hpp"

namespace password_manager {

namespace {

// Passwords longer than this constant should not be checked for weakness using
// the zxcvbn-cpp library. This is because the runtime grows extremely, starting
// at a password length of 40.
// See https://github.com/dropbox/zxcvbn#runtime-latency
// Needs to stay in sync with google3 constant: http://shortn/_1ufIF61G4X
constexpr int kZxcvbnLengthCap = 40;

// If the password has a score of 2 or less, this password should be marked as
// weak. The lower the password score, the weaker it is.
constexpr int kHighSeverityScore = 0;
constexpr int kLowSeverityScore = 2;

constexpr int kStrongPasswordScore = 4;

// Very rough, extremely simplified strength check that only makes sense for
// long passwords.
int SimpleLongPasswordStrengthEstimate(const base::string16& password) {
  base::flat_set<base::char16> chars;

  for (auto character : password) {
    chars.insert(character);
    if (chars.size() > 4) {
      return kStrongPasswordScore;
    }
  }
  return kHighSeverityScore;
}

// Returns the |password| score.
int PasswordWeakCheck(const base::string16& password) {
  // zxcvbn's computation time explodes for long passwords, so don't use it for
  // those.
  if (password.size() > kZxcvbnLengthCap) {
    return SimpleLongPasswordStrengthEstimate(password);
  }
  std::vector<zxcvbn::Match> matches =
      zxcvbn::omnimatch(base::UTF16ToUTF8(password));
  zxcvbn::ScoringResult result = zxcvbn::most_guessable_match_sequence(
      base::UTF16ToUTF8(password), matches);
  return zxcvbn::estimate_attack_times(result.guesses).score;
}

}  // namespace

base::flat_set<base::string16> BulkWeakCheck(
    SavedPasswordsPresenter::SavedPasswordsView saved_passwords) {
  std::vector<base::string16> weak_passwords;

  for (const auto& password : saved_passwords) {
    if (PasswordWeakCheck(password.password_value) <= kLowSeverityScore) {
      weak_passwords.push_back(password.password_value);
    }
  }

  return base::flat_set<base::string16>(std::move(weak_passwords));
}

}  // namespace password_manager
