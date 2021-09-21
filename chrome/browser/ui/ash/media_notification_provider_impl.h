// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include <map>

#include "ash/public/cpp/media_notification_provider.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class MediaNotificationService;
class MediaNotificationListView;
class Profile;

class MediaNotificationProviderImpl
    : public ash::MediaNotificationProvider,
      public global_media_controls::MediaDialogDelegate,
      public global_media_controls::MediaItemManagerObserver,
      public global_media_controls::MediaItemUIObserver,
      public session_manager::SessionManagerObserver {
 public:
  MediaNotificationProviderImpl();
  ~MediaNotificationProviderImpl() override;

  // ash::MediaNotificationProvider:
  void AddObserver(ash::MediaNotificationProviderObserver* observer) override;
  void RemoveObserver(
      ash::MediaNotificationProviderObserver* observer) override;
  bool HasActiveNotifications() override;
  bool HasFrozenNotifications() override;
  std::unique_ptr<views::View> GetMediaNotificationListView(
      int separator_thickness) override;
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override;
  void OnBubbleClosing() override;
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override;

  // global_media_controls::MediaDialogDelegate:
  global_media_controls::MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaItem(const std::string& id) override;
  void HideMediaDialog() override {}
  void Focus() override {}

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() override;
  void OnMediaDialogOpened() override {}
  void OnMediaDialogClosed() override {}

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUISizeChanged() override;
  void OnMediaItemUIDestroyed(const std::string& id) override;

  // session_manager::SessionManagerObserver:
  void OnUserProfileLoaded(const AccountId& account_id) override;

  MediaNotificationService* service_for_testing() { return service_; }

 private:
  base::ObserverList<ash::MediaNotificationProviderObserver> observers_;

  MediaNotificationListView* active_session_view_ = nullptr;

  Profile* profile_ = nullptr;

  MediaNotificationService* service_ = nullptr;

  global_media_controls::MediaItemManager* item_manager_ = nullptr;

  std::map<const std::string, global_media_controls::MediaItemUI*>
      observed_item_uis_;

  absl::optional<media_message_center::NotificationTheme> color_theme_;
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
