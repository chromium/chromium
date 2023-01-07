// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_STRING_CONVERSIONS_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_STRING_CONVERSIONS_H_

#include "components/sync/base/passphrase_enums.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/sync_encryption_handler.h"

namespace syncer {

enum class PassphraseType;

const char* ConnectionStatusToString(ConnectionStatus status);

const char* PassphraseTypeToString(PassphraseType type);

const char* KeyDerivationMethodToString(KeyDerivationMethod method);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_STRING_CONVERSIONS_H_
