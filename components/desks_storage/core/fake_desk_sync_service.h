// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_FAKE_DESK_SYNC_SERVICE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_FAKE_DESK_SYNC_SERVICE_H_

#include "base/logging.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/desks_storage/core/fake_desk_sync_bridge.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"

namespace desks_storage {
class FakeDeskSyncBridge;

// KeyedService responsible for desk templates sync.
class FakeDeskSyncService : public DeskSyncService {
 public:
  explicit FakeDeskSyncService(bool skip_engine_connection = false);
  FakeDeskSyncService(const FakeDeskSyncService&) = delete;
  FakeDeskSyncService& operator=(const FakeDeskSyncService&) = delete;
  ~FakeDeskSyncService() override;

  DeskModel* GetDeskModel() override;
  FakeDeskSyncBridge* GetDeskSyncBridge();
  void SetDeskSyncBridge(FakeDeskSyncBridge* fake_desk_sync_bridge);

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  std::unique_ptr<FakeDeskSyncBridge> fake_bridge_;
  syncer::FakeDataTypeControllerDelegate fake_data_type_controller_delegate_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_FAKE_DESK_SYNC_SERVICE_H_
