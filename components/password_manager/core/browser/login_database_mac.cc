// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include "components/os_crypt/sync/os_crypt.h"

namespace password_manager {

LoginDatabase::EncryptionResult LoginDatabase::EncryptedString(
    const std::u16string& plain_text,
    std::string* cipher_text) {
  return OSCrypt::EncryptString16(plain_text, cipher_text)
             ? ENCRYPTION_RESULT_SUCCESS
             : ENCRYPTION_RESULT_SERVICE_FAILURE;
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    std::u16string* plain_text) {
  return OSCrypt::DecryptString16(cipher_text, plain_text)
             ? ENCRYPTION_RESULT_SUCCESS
             : ENCRYPTION_RESULT_SERVICE_FAILURE;
}

}  // namespace password_manager
