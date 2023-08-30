// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_client_utils.h"

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "components/sync/service/local_data_description.h"

namespace browser_sync::sync_client_utils {

void GetLocalDataDescriptions(
    syncer::ModelTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback,
    DataTypeModels local_and_account_models) {
  // TODO(crbug.com/1451508): Implement this.
  NOTIMPLEMENTED();
}

void TriggerLocalDataMigration(syncer::ModelTypeSet types,
                               DataTypeModels local_and_account_models) {
  // TODO(crbug.com/1451508): Implement this.
  NOTIMPLEMENTED();
}

}  // namespace browser_sync::sync_client_utils
