// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_local_data_batch_uploader.h"

#include <utility>

#include "base/functional/callback.h"
#include "components/sync/service/local_data_description.h"

namespace password_manager {

void PasswordLocalDataBatchUploader::GetLocalDataDescription(
    base::OnceCallback<void(syncer::LocalDataDescription)> callback) {
  std::move(callback).Run({});
}

void PasswordLocalDataBatchUploader::TriggerLocalDataMigration() {}

}  // namespace password_manager
