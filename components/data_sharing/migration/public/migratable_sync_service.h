// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_SERVICE_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_SERVICE_H_

#include "base/observer_list_types.h"
#include "components/data_sharing/migration/public/context_id.h"

namespace data_sharing {

// The primary interface for a feature service that can participate in the
// client-side sharing migration framework. It defines the commands that the
// MigratableSyncServiceCoordinator can issue to the service.
//
// A concrete implementation of this interface corresponds to a single
// user-facing feature (e.g., Tab Groups, Bookmarks). The service is responsible
// for managing the migration of all underlying sync data types associated with
// that feature as a single, cohesive unit.
class MigratableSyncService : public base::CheckedObserver {
 public:
  ~MigratableSyncService() override = default;

  // Called by the coordinator to instruct the service to stage a migration.
  // The service should copy private data, convert it, and write it to the
  // shared bridge. The success of this operation would be communicated to
  // coordinator by the underlying bridge/mediator layers.
  virtual void StageMigration(const ContextId& context_id) = 0;

  // Called by the coordinator to instruct the service to commit a migration.
  // The service should promote the staged shared data to the feature model
  // and (if it's the initiating client) delete the private data.
  //
  // This method should only be called by the MigratableSyncServiceCoordinator.
  // The framework guarantees that this will only be invoked after
  // `IsPromotionReady()` has returned true for this service. Failures during
  // staging are handled by the underlying bridges (e.g., via retries), and
  // the service will not be considered "ready" until staging succeeds.
  virtual void CommitMigration(const ContextId& context_id) = 0;

  // Called by the coordinator to check if the service has received the
  // minimum viable set of shared data to complete a migration.
  //
  // The coordinator calls this method to poll the service's state. This
  // polling is triggered by external events (e.g., receiving a sync update),
  // not a continuous loop.
  virtual bool IsPromotionReady() const = 0;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_MIGRATABLE_SYNC_SERVICE_H_
