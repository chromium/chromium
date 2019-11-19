// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/login_database.h"

namespace password_manager {

namespace {

enum class PasswordDecryptionResult {
  kFailed = 0,
  kSucceeded = 1,
  kSucceededBySkipping = 2,
  kSucceededByIgnoringFailure = 3,
  kMaxValue = kSucceededByIgnoringFailure
};

void RecordPasswordDecryptionResult(PasswordDecryptionResult result) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.StoreDecryptionResult", result);
}

}  // namespace

LoginDatabase::EncryptionResult LoginDatabase::EncryptedString(
    const base::string16& plain_text,
    std::string* cipher_text) const {
#if !defined(OS_FUCHSIA)
  if (!use_encryption_) {
    *cipher_text = base::UTF16ToUTF8(plain_text);
    return ENCRYPTION_RESULT_SUCCESS;
  }
#endif

  return OSCrypt::EncryptString16(plain_text, cipher_text)
             ? ENCRYPTION_RESULT_SUCCESS
             : ENCRYPTION_RESULT_SERVICE_FAILURE;
}

LoginDatabase::EncryptionResult LoginDatabase::DecryptedString(
    const std::string& cipher_text,
    base::string16* plain_text) const {
#if !defined(OS_FUCHSIA)
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // On Android and ChromeOS, we have a mix of obfuscated and plain-text
  // passwords. Obfuscated passwords always start with "v10", therefore anything
  // else is plain-text.
  // TODO(crbug.com/960322): Remove this when there isn't a mix of plain-text
  // and obfuscated passwords.
  bool use_encryption = use_encryption_ && (cipher_text.find("v10", 0) == 0);
#else
  bool use_encryption = use_encryption_;
#endif

  if (!use_encryption) {
    *plain_text = base::UTF8ToUTF16(cipher_text);
    RecordPasswordDecryptionResult(
        PasswordDecryptionResult::kSucceededBySkipping);
    return ENCRYPTION_RESULT_SUCCESS;
  }
#endif  // !defined(OS_FUCHSIA)

  bool decryption_success = OSCrypt::DecryptString16(cipher_text, plain_text);
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // If decryption failed, we assume it was because the value was actually a
  // plain-text password which started with "v10".
  // TODO(crbug.com/960322): Remove this when there isn't a mix of plain-text
  // and obfuscated passwords.
  if (!decryption_success) {
    *plain_text = base::UTF8ToUTF16(cipher_text);
    RecordPasswordDecryptionResult(
        PasswordDecryptionResult::kSucceededByIgnoringFailure);
    return ENCRYPTION_RESULT_SUCCESS;
  }
#endif
  RecordPasswordDecryptionResult(decryption_success
                                     ? PasswordDecryptionResult::kSucceeded
                                     : PasswordDecryptionResult::kFailed);
  return decryption_success ? ENCRYPTION_RESULT_SUCCESS
                            : ENCRYPTION_RESULT_SERVICE_FAILURE;
}

}  // namespace password_manager
