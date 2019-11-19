// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_string_conversions.h"

#define ENUM_CASE(x) \
  case x:            \
    return #x

namespace syncer {

const char* ConnectionStatusToString(ConnectionStatus status) {
  switch (status) {
    ENUM_CASE(CONNECTION_NOT_ATTEMPTED);
    ENUM_CASE(CONNECTION_OK);
    ENUM_CASE(CONNECTION_AUTH_ERROR);
    ENUM_CASE(CONNECTION_SERVER_ERROR);
  }

  NOTREACHED();
  return "INVALID_CONNECTION_STATUS";
}

// Helper function that converts a PassphraseRequiredReason value to a string.
const char* PassphraseRequiredReasonToString(PassphraseRequiredReason reason) {
  switch (reason) {
    ENUM_CASE(REASON_ENCRYPTION);
    ENUM_CASE(REASON_DECRYPTION);
  }

  NOTREACHED();
  return "INVALID_REASON";
}

const char* PassphraseTypeToString(PassphraseType type) {
  switch (type) {
    ENUM_CASE(PassphraseType::kImplicitPassphrase);
    ENUM_CASE(PassphraseType::kKeystorePassphrase);
    ENUM_CASE(PassphraseType::kFrozenImplicitPassphrase);
    ENUM_CASE(PassphraseType::kCustomPassphrase);
    ENUM_CASE(PassphraseType::kTrustedVaultPassphrase);
  }

  NOTREACHED();
  return "INVALID_PASSPHRASE_TYPE";
}

const char* BootstrapTokenTypeToString(BootstrapTokenType type) {
  switch (type) {
    ENUM_CASE(PASSPHRASE_BOOTSTRAP_TOKEN);
    ENUM_CASE(KEYSTORE_BOOTSTRAP_TOKEN);
  }

  NOTREACHED();
  return "INVALID_BOOTSTRAP_TOKEN_TYPE";
}

const char* KeyDerivationMethodToString(KeyDerivationMethod method) {
  switch (method) {
    ENUM_CASE(KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003);
    ENUM_CASE(KeyDerivationMethod::SCRYPT_8192_8_11);
    ENUM_CASE(KeyDerivationMethod::UNSUPPORTED);
  }

  NOTREACHED();
  return "INVALID_KEY_DERIVATION_METHOD";
}

#undef ENUM_CASE

}  // namespace syncer
