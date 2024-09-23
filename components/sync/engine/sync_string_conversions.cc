// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_string_conversions.h"

#include "base/notreached.h"
#include "components/sync/base/passphrase_enums.h"

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

  NOTREACHED_IN_MIGRATION();
  return "INVALID_CONNECTION_STATUS";
}

const char* PassphraseTypeToString(PassphraseType type) {
  switch (type) {
    ENUM_CASE(PassphraseType::kImplicitPassphrase);
    ENUM_CASE(PassphraseType::kKeystorePassphrase);
    ENUM_CASE(PassphraseType::kFrozenImplicitPassphrase);
    ENUM_CASE(PassphraseType::kCustomPassphrase);
    ENUM_CASE(PassphraseType::kTrustedVaultPassphrase);
  }

  NOTREACHED_IN_MIGRATION();
  return "INVALID_PASSPHRASE_TYPE";
}

#undef ENUM_CASE

}  // namespace syncer
