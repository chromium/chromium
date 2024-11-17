// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service.h"

#include "ash/shell.h"
#include "components/global_media_controls/public/media_item_manager.h"

CastMediaNotificationProducerKeyedService::
    CastMediaNotificationProducerKeyedService(Profile* profile)
    : profile_(profile) {
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->AddShellObserver(this);
  }
}

CastMediaNotificationProducerKeyedService::
    ~CastMediaNotificationProducerKeyedService() = default;

void CastMediaNotificationProducerKeyedService::AddMediaItemManager(
    global_media_controls::MediaItemManager* media_item_manager) {
  CHECK(media_item_manager);
  RemoveMediaItemManager(media_item_manager);
  managers_and_producers_[media_item_manager] =
      std::make_unique<CastMediaNotificationProducer>(profile_,
                                                      media_item_manager);
  media_item_manager->AddItemProducer(
      managers_and_producers_[media_item_manager].get());
}

void CastMediaNotificationProducerKeyedService::RemoveMediaItemManager(
    global_media_controls::MediaItemManager* media_item_manager) {
  // Reset() may have already removed it.
  if (base::Contains(managers_and_producers_, media_item_manager)) {
    media_item_manager->RemoveItemProducer(
        managers_and_producers_[media_item_manager].get());
    managers_and_producers_.erase(media_item_manager);
  }
}

void CastMediaNotificationProducerKeyedService::Shutdown() {
  Reset();
}

void CastMediaNotificationProducerKeyedService::OnShellDestroying() {
  Reset();
}

void CastMediaNotificationProducerKeyedService::Reset() {
  for (const auto& entry : managers_and_producers_) {
    entry.first->RemoveItemProducer(entry.second.get());
  }
  managers_and_producers_.clear();
  if (ash::Shell::HasInstance()) {
    ash::Shell::Get()->RemoveShellObserver(this);
  }
}
