// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRING_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRING_H_

#include <cstddef>
#include <string>
#include <variant>

#include "crypto/process_bound_string.h"

namespace password_manager {

// Abstraction class for holding a password string after it's been read from the
// database. Provides an interface mechanism to obfuscate/encrypt passwords in
// memory without forcing a particular implementation.
//
// Current implementation uses crypto::ProcessBoundU16String to protect
// password in memory. Currently this protection is behind the feature flag
// |kUseProcessBoundPasswordString| for a controlled rollout
class PasswordString {
 public:
  PasswordString();
  explicit PasswordString(std::u16string&& plaintext);

  PasswordString(PasswordString&&) noexcept;
  PasswordString& operator=(PasswordString&&) noexcept;

  PasswordString(const PasswordString&) = delete;
  PasswordString& operator=(const PasswordString&) = delete;

  ~PasswordString();

  // Returns the password as a `crypto::SecureU16String`. This is the preferred
  // read path: callers that only need to compare, fill, or hash the password
  // should use this so the plaintext lifetime is minimized.
  crypto::SecureU16String secure_value() const;

  // Returns the password as a plain `std::u16string`. Prefer `secure_value()`
  // when possible.
  std::u16string value() const;

  // Returns true if the password is empty. Does not decrypt.
  bool empty() const;

  // Returns the length of the password. Does not decrypt.
  size_t size() const;

  friend bool operator==(const PasswordString& lhs, const PasswordString& rhs);
  friend bool operator==(const PasswordString& lhs,
                         const crypto::SecureU16String& rhs);
  friend bool operator==(const PasswordString& lhs, const std::u16string& rhs);

 private:
  std::variant<std::u16string, crypto::ProcessBoundU16String> value_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRING_H_
