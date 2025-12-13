// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_BASE_MIGRATABLE_SYNC_SERVICE_H_
#define COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_BASE_MIGRATABLE_SYNC_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/data_sharing/migration/public/context_id.h"
#include "components/data_sharing/migration/public/migratable_sync_service.h"

namespace data_sharing {

class MigratableBridgeMediator;

// A reusable base class for MigratableSyncService implementations. It handles
// owning the bridges and the mediator and forwarding the migration commands.
class COMPONENT_EXPORT(DATA_SHARING_MIGRATION) BaseMigratableSyncService
    : public MigratableSyncService {
 public:
  explicit BaseMigratableSyncService(
      std::unique_ptr<MigratableBridgeMediator> mediator);
  ~BaseMigratableSyncService() override;

  // MigratableSyncService implementation:
  void StageMigration(const ContextId& context_id) override;
  void CommitMigration(const ContextId& context_id) override;
  bool IsPromotionReady() const override;

 private:
  std::unique_ptr<MigratableBridgeMediator> mediator_;

  base::WeakPtrFactory<BaseMigratableSyncService> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_MIGRATION_PUBLIC_BASE_MIGRATABLE_SYNC_SERVICE_H_
