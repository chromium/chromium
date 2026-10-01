// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRING_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRING_H_

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
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

  PasswordString(const PasswordString&);
  PasswordString& operator=(const PasswordString&);

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

  void clear();

  struct TransparentHash {
    using is_transparent = void;

    size_t operator()(const PasswordString& password) const {
      if (std::holds_alternative<crypto::ProcessBoundU16String>(
              password.value_)) {
        return operator()(
            std::get<crypto::ProcessBoundU16String>(password.value_)
                .secure_value());
      }
      return operator()(std::get<std::u16string>(password.value_));
    }

    size_t operator()(std::u16string_view password) const {
      return std::hash<std::u16string_view>{}(password);
    }
  };

  struct TransparentEqual {
    using is_transparent = void;

    bool operator()(const PasswordString& lhs,
                    const PasswordString& rhs) const {
      return lhs == rhs;
    }

    bool operator()(const PasswordString& lhs, std::u16string_view rhs) const {
      if (lhs.size() != rhs.size()) {
        return false;
      }
      return lhs.secure_value() == rhs;
    }

    bool operator()(std::u16string_view lhs, const PasswordString& rhs) const {
      if (lhs.size() != rhs.size()) {
        return false;
      }
      return lhs == rhs.secure_value();
    }

    bool operator()(std::u16string_view lhs, std::u16string_view rhs) const {
      return lhs == rhs;
    }
  };

  friend bool operator==(const PasswordString& lhs, const PasswordString& rhs);
  friend bool operator==(const PasswordString& lhs,
                         const crypto::SecureU16String& rhs);
  friend bool operator==(const PasswordString& lhs, const std::u16string& rhs);
  friend bool operator==(const PasswordString& lhs, const char16_t* rhs);

  template <typename H>
  friend H AbslHashValue(H h, const PasswordString& password) {
    if (std::holds_alternative<crypto::ProcessBoundU16String>(
            password.value_)) {
      return H::combine(std::move(h),
                        std::get<crypto::ProcessBoundU16String>(password.value_)
                            .secure_value());
    }
    return H::combine(std::move(h), std::get<std::u16string>(password.value_));
  }

  using absl_container_hash = TransparentHash;
  using absl_container_eq = TransparentEqual;

 private:
  std::variant<std::u16string, crypto::ProcessBoundU16String> value_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STRING_H_
