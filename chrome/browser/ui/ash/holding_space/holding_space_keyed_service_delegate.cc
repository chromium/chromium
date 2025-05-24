// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

namespace ash {

HoldingSpaceKeyedServiceDelegate::~HoldingSpaceKeyedServiceDelegate() = default;

void HoldingSpaceKeyedServiceDelegate::NotifyPersistenceRestored() {
  DCHECK(is_restoring_persistence_);
  is_restoring_persistence_ = false;
  OnPersistenceRestored();
}

HoldingSpaceKeyedServiceDelegate::HoldingSpaceKeyedServiceDelegate(
    HoldingSpaceKeyedService* service,
    HoldingSpaceModel* model)
    : service_(service), model_(model) {
  holding_space_model_observation_.Observe(model);
}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemsAdded(
    const std::vector<const HoldingSpaceItem*>& items) {}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemsRemoved(
    const std::vector<const HoldingSpaceItem*>& items) {}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemInitialized(
    const HoldingSpaceItem* item) {}

void HoldingSpaceKeyedServiceDelegate::OnPersistenceRestored() {}

}  // namespace ash
