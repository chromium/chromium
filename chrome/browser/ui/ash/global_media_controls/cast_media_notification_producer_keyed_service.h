// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_KEYED_SERVICE_H_

#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// Owns CastMediaNotificationProducer that depends on MediaRouter (another
// KeyedService) and thus needs to be destroyed during the KeyedServices
// shutdown phase.
class CastMediaNotificationProducerKeyedService : public KeyedService,
                                                  public ash::ShellObserver {
 public:
  explicit CastMediaNotificationProducerKeyedService(Profile* profile);
  CastMediaNotificationProducerKeyedService(
      const CastMediaNotificationProducerKeyedService&) = delete;
  CastMediaNotificationProducerKeyedService& operator=(
      const CastMediaNotificationProducerKeyedService&) = delete;
  ~CastMediaNotificationProducerKeyedService() override;

  // Add the MediaItemManager and create a corresponding
  // CastMediaNotificationProducer for it.
  void AddMediaItemManager(
      global_media_controls::MediaItemManager* media_item_manager);

  // Remove the MediaItemManager and delete the corresponding
  // CastMediaNotificationProducer.
  void RemoveMediaItemManager(
      global_media_controls::MediaItemManager* media_item_manager);

 private:
  // KeyedService:
  void Shutdown() override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  void Reset();

  const raw_ptr<Profile> profile_;

  // A map to record the MediaItemManager pointer and the corresponding
  // CastMediaNotificationProducer.
  std::map<raw_ptr<global_media_controls::MediaItemManager>,
           std::unique_ptr<CastMediaNotificationProducer>>
      managers_and_producers_;
};

#endif  // CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_KEYED_SERVICE_H_
