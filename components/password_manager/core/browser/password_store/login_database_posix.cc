// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/login_database.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/password_manager/core/browser/password_store/login_database.h"

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
#if !BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  // On Android and ChromeOS, we have a mix of obfuscated and plain-text
  // passwords. Obfuscated passwords always start with "v10", therefore anything
  // else is plain-text.
  // TODO(crbug.com/41457193): Remove this when there isn't a mix of plain-text
  // and obfuscated passwords.
  bool use_encryption = base::StartsWith(cipher_text, "v10") || encryptor_;
#else
  bool use_encryption = true;
#endif

  if (!use_encryption) {
    *plain_text = base::UTF8ToUTF16(cipher_text);
    RecordPasswordDecryptionResult(
        PasswordDecryptionResult::kSucceededBySkipping);
    return EncryptionResult::kSuccess;
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)

  bool decryption_success =
      encryptor_ ? encryptor_->DecryptString16(cipher_text, plain_text)
                 : OSCrypt::DecryptString16(cipher_text, plain_text);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH)
  // If decryption failed, we assume it was because the value was actually a
  // plain-text password which started with "v10".
  // TODO(crbug.com/41457193): Remove this when there isn't a mix of plain-text
  // and obfuscated passwords.
  if (!decryption_success) {
    *plain_text = base::UTF8ToUTF16(cipher_text);
    RecordPasswordDecryptionResult(
        PasswordDecryptionResult::kSucceededByIgnoringFailure);
    return EncryptionResult::kSuccess;
  }
#endif
  RecordPasswordDecryptionResult(decryption_success
                                     ? PasswordDecryptionResult::kSucceeded
                                     : PasswordDecryptionResult::kFailed);
  return decryption_success ? EncryptionResult::kSuccess
                            : EncryptionResult::kServiceFailure;
}

}  // namespace password_manager
