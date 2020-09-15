// Copyright 2020 The Chromium Authors. All rights reserved.
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

void HoldingSpaceKeyedServiceDelegate::Shutdown() {}

void HoldingSpaceKeyedServiceDelegate::NotifyDownloadsRestored() {
  DCHECK(is_restoring_downloads_);
  is_restoring_downloads_ = false;
  OnDownloadsRestored();
}

void HoldingSpaceKeyedServiceDelegate::NotifyPersistenceRestored() {
  DCHECK(is_restoring_persistence_);
  is_restoring_persistence_ = false;
  OnPersistenceRestored();
}

HoldingSpaceKeyedServiceDelegate::HoldingSpaceKeyedServiceDelegate(
    Profile* profile,
    HoldingSpaceModel* model)
    : profile_(profile), model_(model) {
  // It is expected that `profile` already be ready prior to delegate creation.
  DCHECK(GetProfileManager()->IsValidProfile(profile));
  holding_space_model_observer_.Add(model);
}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemAdded(
    const HoldingSpaceItem* item) {}

void HoldingSpaceKeyedServiceDelegate::OnHoldingSpaceItemRemoved(
    const HoldingSpaceItem* item) {}

void HoldingSpaceKeyedServiceDelegate::OnDownloadsRestored() {}

void HoldingSpaceKeyedServiceDelegate::OnPersistenceRestored() {}

}  // namespace ash
