// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/media_router/browser/media_routes_observer.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class Profile;

// Manages media notifications shown in the Global Media Controls dialog for
// active Cast sessions.
class CastMediaNotificationProducer
    : public global_media_controls::MediaItemProducer,
      public media_router::MediaRoutesObserver,
      public global_media_controls::MediaItemUIObserver {
 public:
  CastMediaNotificationProducer(
      Profile* profile,
      global_media_controls::MediaItemManager* item_manager);
  CastMediaNotificationProducer(
      Profile* profile,
      media_router::MediaRouter* router,
      global_media_controls::MediaItemManager* item_manager);
  CastMediaNotificationProducer(const CastMediaNotificationProducer&) = delete;
  CastMediaNotificationProducer& operator=(
      const CastMediaNotificationProducer&) = delete;
  ~CastMediaNotificationProducer() override;

  // global_media_controls::MediaItemProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem> GetMediaItem(
      const std::string& id) override;
  std::set<std::string> GetActiveControllableItemIds() const override;
  bool HasFrozenItems() override;
  void OnItemShown(const std::string& id,
                   global_media_controls::MediaItemUI* item_ui) override;
  void OnDialogDisplayed() override;
  bool IsItemActivelyPlaying(const std::string& id) override;

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUIDismissed(const std::string& id) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(
      const std::vector<media_router::MediaRoute>& routes) override;

  size_t GetActiveItemCount() const;
  bool HasLocalMediaRoute() const;

 private:
  using Items = std::map<std::string, CastMediaNotificationItem>;

  bool HasActiveItems() const;

  const raw_ptr<Profile> profile_;
  const raw_ptr<media_router::MediaRouter> router_;
  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;

  // Maps from notification item IDs to items.
  Items items_;

  global_media_controls::MediaItemUIObserverSet item_ui_observer_set_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_PRODUCER_H_
