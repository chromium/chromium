// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/fake_desk_sync_service.h"
#include "base/logging.h"
#include "fake_desk_sync_bridge.h"
#include "fake_desk_sync_service.h"

namespace desks_storage {

FakeDeskSyncService::FakeDeskSyncService(bool skip_engine_connection)
    : fake_data_type_controller_delegate_(syncer::DataType::WORKSPACE_DESK) {
  fake_bridge_ = std::make_unique<FakeDeskSyncBridge>();
  fake_bridge_->SetCacheGuid("test_guid");
  if (skip_engine_connection) {
    fake_data_type_controller_delegate_
        .EnableSkipEngineConnectionForActivationResponse();
  }
}
FakeDeskSyncService::~FakeDeskSyncService() = default;

DeskModel* FakeDeskSyncService::GetDeskModel() {
  return fake_bridge_.get();
}

FakeDeskSyncBridge* FakeDeskSyncService::GetDeskSyncBridge() {
  return fake_bridge_.get();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeDeskSyncService::GetControllerDelegate() {
  return fake_data_type_controller_delegate_.GetWeakPtr();
}

}  // namespace desks_storage
