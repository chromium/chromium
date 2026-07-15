// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace sync_tab_context {

// Controller for TabContext sync datatypes (ENCRYPTED_TAB_CONTEXT_CONTAINER
// and ENCRYPTED_TAB_CONTEXT_ITEM).
class TabContextDataTypeController : public syncer::DataTypeController,
                                     public syncer::SyncServiceObserver {
 public:
  // `sync_service` must not be null and must outlive this object.
  TabContextDataTypeController(
      syncer::DataType type,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      syncer::SyncService* sync_service);

  TabContextDataTypeController(const TabContextDataTypeController&) = delete;
  TabContextDataTypeController& operator=(const TabContextDataTypeController&) =
      delete;

  ~TabContextDataTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  const raw_ptr<syncer::SyncService> sync_service_;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_DATA_TYPE_CONTROLLER_H_
