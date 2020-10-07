// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include "ash/public/cpp/media_notification_provider.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "components/session_manager/core/session_manager_observer.h"

class MediaNotificationService;
class MediaNotificationListView;
class MediaNotificationContainerImplView;

class MediaNotificationProviderImpl
    : public ash::MediaNotificationProvider,
      public MediaDialogDelegate,
      public MediaNotificationServiceObserver,
      public MediaNotificationContainerObserver,
      public session_manager::SessionManagerObserver {
 public:
  MediaNotificationProviderImpl();
  ~MediaNotificationProviderImpl() override;

  // MediaNotificationProvider implementations.
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

  // MediaDialogDelegate implementations.
  MediaNotificationContainerImpl* ShowMediaSession(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaSession(const std::string& id) override;
  std::unique_ptr<OverlayMediaNotification> PopOut(const std::string& id,
                                                   gfx::Rect bounds) override;
  void HideMediaDialog() override {}

  // MediaNotificationServiceObserver implementations.
  void OnNotificationListChanged() override;
  void OnMediaDialogOpened() override {}
  void OnMediaDialogClosed() override {}

  // MediaNotificationContainerObserver implementations.
  void OnContainerSizeChanged() override;
  void OnContainerMetadataChanged() override {}
  void OnContainerActionsChanged() override {}
  void OnContainerClicked(const std::string& id) override {}
  void OnContainerDismissed(const std::string& id) override {}
  void OnContainerDestroyed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override {
  }
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override {}

  // SessionManagerobserver implementation.
  void OnUserProfileLoaded(const AccountId& account_id) override;

  MediaNotificationService* service_for_testing() { return service_; }

 private:
  base::ObserverList<ash::MediaNotificationProviderObserver> observers_;

  MediaNotificationListView* active_session_view_ = nullptr;

  MediaNotificationService* service_ = nullptr;

  std::map<const std::string, MediaNotificationContainerImplView*>
      observed_containers_;

  base::Optional<media_message_center::NotificationTheme> color_theme_;
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
