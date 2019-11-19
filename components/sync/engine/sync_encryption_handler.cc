// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_encryption_handler.h"

namespace syncer {

SyncEncryptionHandler::Observer::Observer() {}
SyncEncryptionHandler::Observer::~Observer() {}

SyncEncryptionHandler::SyncEncryptionHandler() {}
SyncEncryptionHandler::~SyncEncryptionHandler() {}

// Static.
ModelTypeSet SyncEncryptionHandler::SensitiveTypes() {
  ModelTypeSet types;
  types.Put(PASSWORDS);  // Has its own encryption, but include it anyway.
  types.Put(
      WIFI_CONFIGURATIONS);  // Has its own encryption, but include it anyway.
  return types;
}

}  // namespace syncer
