// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/weak_check_utility.h"

#include <functional>
#include <string_view>

#include "base/containers/span.h"
#include "base/i18n/break_iterator.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_string.h"
#include "crypto/secure_util.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/matching.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/scoring.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/time_estimates.hpp"

namespace password_manager {

std::u16string_view SafeTruncateUTF16(std::u16string_view str,
                                      size_t max_length) {
  if (str.length() <= max_length) {
    return str;
  }

  base::i18n::BreakIterator iter(str,
                                 base::i18n::BreakIterator::BREAK_CHARACTER);
  if (!iter.Init()) {
    return str.substr(0, max_length);
  }

  size_t char_count = 0;
  while (iter.Advance() && char_count < max_length) {
    char_count++;
  }
  return str.substr(0, iter.prev());
}

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PasswordWeaknessScore {
  kTooGuessablePassword = 0,
  kVeryGuessablePassword = 1,
  kSomewhatGuessablePassword = 2,
  kSafelyUnguessablePassword = 3,
  kVeryUnguessablePassword = 4,
  kMaxValue = kVeryUnguessablePassword,
};

// Passwords longer than this constant should not be checked for weakness using
// the zxcvbn-cpp library. This is because the runtime grows extremely, starting
// at a password length of 40.
// See https://github.com/dropbox/zxcvbn#runtime-latency
// Needs to stay in sync with google3 constant: http://shortn/_1ufIF61G4X
constexpr int kZxcvbnLengthCap = 40;

// If the password has a score of 2 or less, this password should be marked as
// weak. The lower the password score, the weaker it is.
constexpr int kLowSeverityScore = 2;

// Returns the |password| score.
int PasswordWeakCheck(std::u16string_view plaintext) {
  std::string password =
      base::UTF16ToUTF8(SafeTruncateUTF16(plaintext, kZxcvbnLengthCap));
  absl::Cleanup password_cleanup = [&password]() {
    // SecureZero the plain text when done with it.
    crypto::SecureZeroBuffer(base::as_writable_byte_span(password));
  };
  // zxcvbn's computation time explodes for long passwords, so cap at that
  // number. Hold the decrypted password in a self-zeroing buffer so the
  // main plaintext is wiped when this scope exits. zxcvbn::Match objects do
  // hold substrings of the password and most_guessable_match_sequence copies
  // more substrings. Currently these heap buffers are never zeroed so the
  // self-zeroing buffer does not perfect wipe all traces of the plaintext.
  // TODO(crbug.com/513276101): Investigate how to purge the |zxcvbn|'s
  // internal copies.
  std::vector<zxcvbn::Match> matches = zxcvbn::omnimatch(password);
  zxcvbn::ScoringResult result =
      zxcvbn::most_guessable_match_sequence(password, matches);

  int score = zxcvbn::estimate_attack_times(result.guesses).score;
  base::UmaHistogramEnumeration("PasswordManager.WeakCheck.PasswordScore",
                                static_cast<PasswordWeaknessScore>(score));

  return score;
}

int PasswordWeakCheck(const PasswordString& password_string) {
  // zxcvbn's computation time explodes for long passwords, so cap at that
  // number. Hold the decrypted password in a self-zeroing buffer so the
  // main plaintext is wiped when this scope exits. zxcvbn::Match objects do
  // hold substrings of the password and most_guessable_match_sequence copies
  // more substrings. Currently these heap buffers are never zeroed so the
  // self-zeroing buffer does not perfect wipe all traces of the plaintext.
  // TODO(crbug.com/513276101): Investigate how to purge the |zxcvbn|'s
  // internal copies.
  const crypto::SecureU16String plaintext = password_string.secure_value();
  return PasswordWeakCheck(plaintext);
}

}  // namespace

IsWeakPassword IsWeak(const PasswordString& password) {
  return IsWeakPassword(PasswordWeakCheck(password) <= kLowSeverityScore);
}

IsWeakPassword IsWeak(std::u16string password) {
  absl::Cleanup password_cleanup = [&password]() {
    // SecureZero the plain text when done with it.
    crypto::SecureZeroBuffer(base::as_writable_byte_span(password));
  };
  return IsWeakPassword(PasswordWeakCheck(password) <= kLowSeverityScore);
}

absl::flat_hash_set<PasswordString> BulkWeakCheck(
    absl::flat_hash_set<PasswordString> passwords) {
  base::UmaHistogramCounts1000("PasswordManager.WeakCheck.CheckedPasswords",
                               passwords.size());
  absl::erase_if(passwords, [](const PasswordString& password) {
    return !IsWeak(password).value();
  });
  base::UmaHistogramCounts1000("PasswordManager.WeakCheck.WeakPasswords",
                               passwords.size());
  return passwords;
}

}  // namespace password_manager
