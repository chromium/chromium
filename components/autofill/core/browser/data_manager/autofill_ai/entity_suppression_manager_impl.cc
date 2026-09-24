// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_manager_impl.h"

#include <algorithm>
#include <vector>

#include "base/check.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_entry.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

EntitySuppressionManagerImpl::EntitySuppressionManagerImpl(
    std::unique_ptr<EntitySuppressionSyncBridge> sync_bridge)
    : sync_bridge_(std::move(sync_bridge)) {
  CHECK(sync_bridge_);
  bridge_observation_.Observe(sync_bridge_.get());
}

EntitySuppressionManagerImpl::~EntitySuppressionManagerImpl() = default;

void EntitySuppressionManagerImpl::AddObserver(
    EntitySuppressionManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void EntitySuppressionManagerImpl::RemoveObserver(
    EntitySuppressionManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool EntitySuppressionManagerImpl::SuppressEntity(
    const EntityInstance& entity) {
  bool modified = false;
  for (const EntitySuppressionEntry& entry :
       GetEntitySuppressionEntries(entity)) {
    if (sync_bridge_->Suppress(entry)) {
      modified = true;
    }
  }
  return modified;
}

bool EntitySuppressionManagerImpl::UnsuppressEntity(
    const EntityInstance& entity) {
  bool modified = false;
  for (const EntitySuppressionEntry& entry :
       GetEntitySuppressionEntries(entity)) {
    if (sync_bridge_->Unsuppress(entry)) {
      modified = true;
    }
  }
  return modified;
}

bool EntitySuppressionManagerImpl::ClearAllSuppressions() {
  return sync_bridge_->ClearAllSuppressions();
}

bool EntitySuppressionManagerImpl::IsSuppressed(
    const EntityInstance& entity) const {
  return std::ranges::any_of(GetEntitySuppressionEntries(entity),
                             [this](const EntitySuppressionEntry& entry) {
                               return sync_bridge_->IsSuppressed(entry);
                             });
}

void EntitySuppressionManagerImpl::OnSuppressionsChanged() {
  observers_.Notify(
      &EntitySuppressionManager::Observer::OnEntitySuppressionsChanged);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
EntitySuppressionManagerImpl::GetSyncControllerDelegate() {
  return sync_bridge_->GetControllerDelegate();
}

}  // namespace autofill
