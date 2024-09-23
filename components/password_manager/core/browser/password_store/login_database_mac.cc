// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database.h"

#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"

namespace password_manager {

EncryptionResult LoginDatabase::EncryptedString(
    const std::u16string& plain_text,
    std::string* cipher_text) const {
  bool result = encryptor_
                    ? encryptor_->EncryptString16(plain_text, cipher_text)
                    : OSCrypt::EncryptString16(plain_text, cipher_text);
  return result ? EncryptionResult::kSuccess
                : EncryptionResult::kServiceFailure;
}

EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    std::u16string* plain_text) const {
  bool result = encryptor_
                    ? encryptor_->DecryptString16(cipher_text, plain_text)
                    : OSCrypt::DecryptString16(cipher_text, plain_text);

  return result ? EncryptionResult::kSuccess
                : EncryptionResult::kServiceFailure;
}

}  // namespace password_manager
