// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_string.h"

#include <string>
#include <utility>
#include <variant>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "crypto/process_bound_string.h"
#include "crypto/secure_util.h"

namespace password_manager {

namespace {

bool UseProcessBoundBacking() {
  return base::FeatureList::IsEnabled(features::kUseProcessBoundPasswordString);
}

}  // namespace

PasswordString::PasswordString() : PasswordString(std::u16string()) {}

PasswordString::PasswordString(std::u16string&& plaintext) {
  // To ensure that |plaintext| is purged of the password after the
  // PasswordString object is created, the constructor takes an R-Value.
  // crypto::ProcessBoundU16String takes a const ref and so move semantics
  // cannot be used for that pathway to forcibly clean |plaintext|. As a result,
  // the constructor foricbly sets the bits of |plaintext| to 0 using
  // |crypto::SecureZeroBuffer| before returning. This ensures that |plaintext|
  // is purged of the plain text password string, removing it from memory now
  // that PasswordString owns it, and satisfies the R-value argument contract.
  if (UseProcessBoundBacking()) {
    value_ = crypto::ProcessBoundU16String(plaintext);
  } else {
    value_ = plaintext;
  }
  crypto::SecureZeroBuffer(base::as_writable_byte_span(plaintext));
}

PasswordString::PasswordString(PasswordString&&) noexcept = default;
PasswordString& PasswordString::operator=(PasswordString&&) noexcept = default;

PasswordString::~PasswordString() = default;

crypto::SecureU16String PasswordString::secure_value() const {
  if (std::holds_alternative<crypto::ProcessBoundU16String>(value_)) {
    return std::get<crypto::ProcessBoundU16String>(value_).secure_value();
  }

  CHECK(std::holds_alternative<std::u16string>(value_));
  const std::u16string& plain_text = std::get<std::u16string>(value_);
  return crypto::SecureU16String(plain_text.begin(), plain_text.end());
}

std::u16string PasswordString::value() const {
  if (std::holds_alternative<crypto::ProcessBoundU16String>(value_)) {
    return std::get<crypto::ProcessBoundU16String>(value_).value();
  }

  CHECK(std::holds_alternative<std::u16string>(value_));
  return std::get<std::u16string>(value_);
}

bool PasswordString::empty() const {
  if (std::holds_alternative<crypto::ProcessBoundU16String>(value_)) {
    return std::get<crypto::ProcessBoundU16String>(value_).empty();
  }

  CHECK(std::holds_alternative<std::u16string>(value_));
  return std::get<std::u16string>(value_).empty();
}

size_t PasswordString::size() const {
  if (std::holds_alternative<crypto::ProcessBoundU16String>(value_)) {
    return std::get<crypto::ProcessBoundU16String>(value_).size();
  }

  CHECK(std::holds_alternative<std::u16string>(value_));
  return std::get<std::u16string>(value_).size();
}

bool operator==(const PasswordString& lhs, const PasswordString& rhs) {
  // Short circuit return false if the sizes are different. This avoids
  // unnecessary decryptions.
  if (lhs.size() != rhs.size()) {
    return false;
  }
  return lhs.secure_value() == rhs.secure_value();
}

bool operator==(const PasswordString& lhs, const crypto::SecureU16String& rhs) {
  // Short circuit return false if the sizes are different. This avoids
  // unnecessary decryptions.
  if (lhs.size() != rhs.size()) {
    return false;
  }
  return lhs.secure_value() == rhs;
}
bool operator==(const PasswordString& lhs, const std::u16string& rhs) {
  // Short circuit return false if the sizes are different. This avoids
  // unnecessary decryptions.
  if (lhs.size() != rhs.size()) {
    return false;
  }
  // Use "value()" instead of "secure_value()" to satisfy the typing, but then
  // manually purge the decrypted value from memory.
  std::u16string value = lhs.value();
  bool result = value == rhs;
  crypto::SecureZeroBuffer(base::as_writable_byte_span(value));
  return result;
}

}  // namespace password_manager
