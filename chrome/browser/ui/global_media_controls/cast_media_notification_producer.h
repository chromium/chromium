// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer_set.h"
#include "chrome/browser/ui/global_media_controls/media_notification_producer.h"
#include "components/media_router/browser/media_routes_observer.h"

class Profile;

namespace media_message_center {
class MediaNotificationController;
}  // namespace media_message_center

// Manages media notifications shown in the Global Media Controls dialog for
// active Cast sessions.
class CastMediaNotificationProducer
    : public MediaNotificationProducer,
      public media_router::MediaRoutesObserver,
      public MediaNotificationContainerObserver {
 public:
  CastMediaNotificationProducer(
      Profile* profile,
      media_message_center::MediaNotificationController*
          notification_controller,
      base::RepeatingClosure items_changed_callback);
  CastMediaNotificationProducer(
      Profile* profile,
      media_router::MediaRouter* router,
      media_message_center::MediaNotificationController*
          notification_controller,
      base::RepeatingClosure items_changed_callback_);
  CastMediaNotificationProducer(const CastMediaNotificationProducer&) = delete;
  CastMediaNotificationProducer& operator=(
      const CastMediaNotificationProducer&) = delete;
  ~CastMediaNotificationProducer() override;

  // MediaNotificationProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id) override;
  std::set<std::string> GetActiveControllableNotificationIds() const override;
  void OnItemShown(const std::string& id,
                   MediaNotificationContainerImpl* container) override;

  // MediaNotificationContainerObserver:
  void OnContainerDismissed(const std::string& id) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const std::vector<media_router::MediaRoute>& routes,
                       const std::vector<media_router::MediaRoute::Id>&
                           joinable_route_ids) override;

  size_t GetActiveItemCount() const;

 private:
  bool HasActiveItems() const;

  Profile* const profile_;
  media_router::MediaRouter* const router_;
  media_message_center::MediaNotificationController* const
      notification_controller_;

  // Maps from notification item IDs to items.
  std::map<std::string, CastMediaNotificationItem> items_;

  // Called when the number of items changes from zero to positive or vice
  // versa.
  base::RepeatingClosure items_changed_callback_;

  MediaNotificationContainerObserverSet container_observer_set_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_H_
