// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/migration/public/base_migratable_sync_service.h"

#include <utility>

#include "base/notimplemented.h"
#include "components/data_sharing/migration/public/context_id.h"
#include "components/data_sharing/migration/public/migratable_bridge_mediator.h"

namespace data_sharing {

BaseMigratableSyncService::BaseMigratableSyncService(
    std::unique_ptr<MigratableBridgeMediator> mediator)
    : mediator_(std::move(mediator)) {}

BaseMigratableSyncService::~BaseMigratableSyncService() = default;

void BaseMigratableSyncService::StageMigration(const ContextId& context_id) {
  if (mediator_) {
    mediator_->StageMigration(context_id);
  }
}

void BaseMigratableSyncService::CommitMigration(const ContextId& context_id) {
  if (mediator_) {
    mediator_->CommitMigration(context_id);
  }
}

bool BaseMigratableSyncService::IsPromotionReady() const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace data_sharing
