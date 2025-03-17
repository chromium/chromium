// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/key.h"

#include <memory>

#include "base/base64.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "crypto/hash.h"
#include "crypto/kdf.h"
#include "crypto/subtle_passkey.h"

namespace ash {

Key::Key() : key_type_(KEY_TYPE_PASSWORD_PLAIN) {}

Key::Key(const Key& other) = default;

Key::Key(const std::string& plain_text_password)
    : key_type_(KEY_TYPE_PASSWORD_PLAIN), secret_(plain_text_password) {}

Key::Key(KeyType key_type, const std::string& salt, const std::string& secret)
    : key_type_(key_type), salt_(salt), secret_(secret) {}

Key::~Key() = default;

bool Key::operator==(const Key& other) const {
  return other.key_type_ == key_type_ && other.salt_ == salt_ &&
         other.secret_ == secret_ && other.label_ == label_;
}

Key::KeyType Key::GetKeyType() const {
  return key_type_;
}

const std::string& Key::GetSecret() const {
  return secret_;
}

const std::string& Key::GetLabel() const {
  return label_;
}

void Key::SetLabel(const std::string& label) {
  label_ = label;
}

void Key::ClearSecret() {
  secret_.clear();
}

void Key::Transform(KeyType target_key_type, const std::string& salt) {
  if (key_type_ != KEY_TYPE_PASSWORD_PLAIN) {
    NOTREACHED();
  }

  std::string salted_secret = salt + secret_;
  switch (target_key_type) {
    case KEY_TYPE_SALTED_SHA256_TOP_HALF: {
      // TODO(stevenjb/nkostylev): Handle empty salt gracefully.
      CHECK(!salt.empty());
      auto hash = crypto::hash::Sha256(base::as_byte_span(salted_secret));

      // Keep only the first half of the hash for 'weak' hashing so that the
      // plain text secret cannot be reconstructed even if the hashing is
      // reversed.
      //
      // Crypto note: this does not make much sense. An exhaustive search for
      // the input secret would just need to check for the first half of the
      // hash matching to have an extremely high probability of being the
      // correct secret anyway, and there's not (nor is there likely to be) any
      // feasible way to invert SHA-256 directly.
      secret_ = base::ToLowerASCII(
          base::HexEncode(base::span(hash).first(hash.size() / 2)));
      break;
    }
    case KEY_TYPE_SALTED_PBKDF2_AES256_1234: {
      std::array<uint8_t, 32> derived;
      crypto::kdf::DeriveKeyPbkdf2HmacSha1(
          {.iterations = 1234}, base::as_byte_span(secret_),
          base::as_byte_span(salt), derived, crypto::SubtlePassKey{});
      secret_ = base::Base64Encode(derived);
      break;
    }
    case KEY_TYPE_SALTED_SHA256:
      secret_ = base::Base64Encode(
          crypto::hash::Sha256(base::as_byte_span(salted_secret)));
      break;

    default:
      // The resulting key will be sent to cryptohomed. It should always be
      // hashed. If hashing fails, crash instead of sending a plain-text key.
      NOTREACHED();
  }

  key_type_ = target_key_type;
  salt_ = salt;
}

}  // namespace ash
