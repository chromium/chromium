// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_SERVICE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/version_info/channel.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace desks_storage {
class DeskSyncBridge;
class DeskModel;

// KeyedService responsible for desk templates sync.
class DeskSyncService : public KeyedService {
 public:
  DeskSyncService();
  DeskSyncService(version_info::Channel channel,
                  syncer::OnceDataTypeStoreFactory create_store_callback,
                  const AccountId& account_id);
  DeskSyncService(const DeskSyncService&) = delete;
  DeskSyncService& operator=(const DeskSyncService&) = delete;
  ~DeskSyncService() override;

  virtual DeskModel* GetDeskModel();

  virtual base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetControllerDelegate();

 private:
  std::unique_ptr<DeskSyncBridge> bridge_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_SYNC_SERVICE_H_
