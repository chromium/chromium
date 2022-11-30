// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/login_database.h"

#include "base/strings/string_util.h"
#include "components/os_crypt/os_crypt.h"

namespace password_manager {

LoginDatabase::EncryptionResult LoginDatabase::EncryptedString(
    const std::u16string& plain_text,
    std::string* cipher_text) {
  if (OSCrypt::EncryptString16(plain_text, cipher_text))
    return ENCRYPTION_RESULT_SUCCESS;
  return ENCRYPTION_RESULT_ITEM_FAILURE;
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    std::u16string* plain_text) {
  // Unittests need to read sample database entries. If these entries had real
  // passwords, their encoding would need to be different for every platform.
  // To avoid the need for that, the entries have empty passwords. OSCrypt on
  // Windows does not recognise the empty string as a valid encrypted string.
  // Changing that for all clients of OSCrypt could have too broad an impact,
  // therefore to allow platform-independent data files for LoginDatabase
  // tests, the special handling of the empty string is added below instead.
  // See also https://codereview.chromium.org/2291123008/#msg14 for a
  // discussion.
  if (cipher_text.empty()) {
    plain_text->clear();
    return ENCRYPTION_RESULT_SUCCESS;
  }
  if (OSCrypt::DecryptString16(cipher_text, plain_text))
    return ENCRYPTION_RESULT_SUCCESS;
  return ENCRYPTION_RESULT_ITEM_FAILURE;
}

}  // namespace password_manager
