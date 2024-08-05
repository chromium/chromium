// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_sync_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_sync_bridge.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"

namespace desks_storage {
DeskSyncService::DeskSyncService() = default;
DeskSyncService::DeskSyncService(
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    const AccountId& account_id) {
  bridge_ = std::make_unique<desks_storage::DeskSyncBridge>(
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::WORKSPACE_DESK,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      std::move(create_store_callback), account_id);
}

DeskSyncService::~DeskSyncService() = default;

DeskModel* DeskSyncService::GetDeskModel() {
  return bridge_.get();
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
DeskSyncService::GetControllerDelegate() {
  return bridge_->change_processor()->GetControllerDelegate();
}

}  // namespace desks_storage
