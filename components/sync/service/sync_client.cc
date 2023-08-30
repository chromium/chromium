// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_client.h"

#include "base/notreached.h"

namespace syncer {

void SyncClient::GetLocalDataDescriptions(
    ModelTypeSet types,
    base::OnceCallback<void(std::map<ModelType, LocalDataDescription>)>
        callback) {
  NOTIMPLEMENTED() << "SyncClient implementations should implement this.";
}

void SyncClient::TriggerLocalDataMigration(ModelTypeSet types) {
  NOTIMPLEMENTED() << "SyncClient implementations should implement this.";
}
}  // namespace syncer
