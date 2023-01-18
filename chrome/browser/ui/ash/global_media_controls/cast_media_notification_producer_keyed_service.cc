// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service.h"

#include "ash/shell.h"
#include "ash/system/media/media_notification_provider.h"
#include "components/global_media_controls/public/media_item_manager.h"

namespace {

global_media_controls::MediaItemManager* GetItemManager() {
  // ash::Shell may be destroyed before Profile, so we need to check if it
  // exists.
  if (!ash::Shell::HasInstance())
    return nullptr;

  return ash::Shell::Get()
      ->media_notification_provider()
      ->GetMediaItemManager();
}

}  // namespace

CastMediaNotificationProducerKeyedService::
    CastMediaNotificationProducerKeyedService(Profile* profile)
    : item_manager_(GetItemManager()) {
  if (!item_manager_)
    return;

  ash::Shell::Get()->AddShellObserver(this);
  cast_producer_ =
      std::make_unique<CastMediaNotificationProducer>(profile, item_manager_);
  item_manager_->AddItemProducer(cast_producer_.get());
}

CastMediaNotificationProducerKeyedService::
    ~CastMediaNotificationProducerKeyedService() = default;

void CastMediaNotificationProducerKeyedService::Shutdown() {
  Reset();
}

void CastMediaNotificationProducerKeyedService::OnShellDestroying() {
  Reset();
}

void CastMediaNotificationProducerKeyedService::Reset() {
  if (!item_manager_) {
    DCHECK(!cast_producer_);
    return;
  }
  item_manager_->RemoveItemProducer(cast_producer_.get());
  item_manager_ = nullptr;
  ash::Shell::Get()->RemoveShellObserver(this);
  // |cast_producer_| depends on both the MediaRouter KeyedService and
  // MediaItemManager (owned by ash::Shell), and must be deleted during
  // KeyedServices shutdown or Shell destruction, whichever comes earlier.
  cast_producer_.reset();
}
