// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/system/media/media_notification_provider.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace global_media_controls {
class MediaItemManager;
class MediaItemUIListView;
class MediaSessionItemProducer;
}  // namespace global_media_controls

namespace media_session {
class MediaSessionService;
}  // namespace media_session

namespace ash {

class ASH_EXPORT MediaNotificationProviderImpl
    : public MediaNotificationProvider,
      public global_media_controls::MediaDialogDelegate,
      public global_media_controls::MediaItemManagerObserver,
      public global_media_controls::MediaItemUIObserver {
 public:
  explicit MediaNotificationProviderImpl(
      media_session::MediaSessionService* service);
  ~MediaNotificationProviderImpl() override;

  // MediaNotificationProvider:
  void AddObserver(MediaNotificationProviderObserver* observer) override;
  void RemoveObserver(MediaNotificationProviderObserver* observer) override;
  bool HasActiveNotifications() override;
  bool HasFrozenNotifications() override;
  std::unique_ptr<views::View> GetMediaNotificationListView(
      int separator_thickness,
      bool should_clip_height) override;
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override;
  void OnBubbleClosing() override;
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override;
  global_media_controls::MediaItemManager* GetMediaItemManager() override;

  // global_media_controls::MediaDialogDelegate:
  global_media_controls::MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaItem(const std::string& id) override;
  void RefreshMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item)
      override {}
  void HideMediaDialog() override {}
  void Focus() override {}

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() override;
  void OnMediaDialogOpened() override {}
  void OnMediaDialogClosed() override {}

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUISizeChanged() override;
  void OnMediaItemUIDestroyed(const std::string& id) override;

  global_media_controls::MediaSessionItemProducer*
  media_session_item_producer_for_testing() {
    return media_session_item_producer_.get();
  }

  void set_profile_for_testing(Profile* profile) {
    profile_for_testing_ = profile;
  }

 private:
  base::ObserverList<MediaNotificationProviderObserver> observers_;

  base::WeakPtr<global_media_controls::MediaItemUIListView>
      active_session_view_;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;

  absl::optional<media_message_center::NotificationTheme> color_theme_;

  global_media_controls::MediaItemUIObserverSet item_ui_observer_set_{this};

  raw_ptr<Profile> profile_for_testing_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
