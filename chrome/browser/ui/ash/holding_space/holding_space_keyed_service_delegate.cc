// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace ash {

namespace {

ProfileManager* GetProfileManager() {
  return g_browser_process->profile_manager();
}

}  // namespace

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
  // It's expected that `profile()` already be ready prior to delegate creation.
  DCHECK(GetProfileManager()->IsValidProfile(profile()));
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
