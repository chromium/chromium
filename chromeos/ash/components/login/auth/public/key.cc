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
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

namespace ash {

namespace {

// Parameters for the transformation to KEY_TYPE_SALTED_AES256_1234.
const int kNumIterations = 1234;
const int kKeySizeInBits = 256;

}  // namespace

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
    NOTREACHED_IN_MIGRATION();
    return;
  }

  switch (target_key_type) {
    case KEY_TYPE_SALTED_SHA256_TOP_HALF: {
      // TODO(stevenjb/nkostylev): Handle empty salt gracefully.
      CHECK(!salt.empty());
      char hash[crypto::kSHA256Length];
      crypto::SHA256HashString(salt + secret_, &hash, sizeof(hash));

      // Keep only the first half of the hash for 'weak' hashing so that the
      // plain text secret cannot be reconstructed even if the hashing is
      // reversed.
      secret_ = base::ToLowerASCII(base::HexEncode(
          reinterpret_cast<const void*>(hash), sizeof(hash) / 2));
      break;
    }
    case KEY_TYPE_SALTED_PBKDF2_AES256_1234: {
      std::unique_ptr<crypto::SymmetricKey> key(
          crypto::SymmetricKey::DeriveKeyFromPasswordUsingPbkdf2(
              crypto::SymmetricKey::AES, secret_, salt, kNumIterations,
              kKeySizeInBits));
      secret_ = base::Base64Encode(key->key());
      break;
    }
    case KEY_TYPE_SALTED_SHA256:
      secret_ = base::Base64Encode(crypto::SHA256HashString(salt + secret_));
      break;

    default:
      // The resulting key will be sent to cryptohomed. It should always be
      // hashed. If hashing fails, crash instead of sending a plain-text key.
      CHECK(false);
      return;
  }

  key_type_ = target_key_type;
  salt_ = salt;
}

}  // namespace ash
