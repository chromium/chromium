// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include "ash/public/cpp/media_notification_provider.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "components/session_manager/core/session_manager_observer.h"

class MediaNotificationService;

class MediaNotificationProviderImpl
    : public ash::MediaNotificationProvider,
      public MediaNotificationServiceObserver,
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
  std::unique_ptr<views::View> GetMediaNotificationListView() override;
  std::unique_ptr<views::View> GetActiveMediaNotificationView() override;

  // MediaNotificationServiceObserver implementations.
  void OnNotificationListChanged() override;
  void OnMediaDialogOpenedOrClosed() override {}

  // SessionManagerobserver implementation.
  void OnUserProfileLoaded(const AccountId& account_id) override;

 private:
  base::ObserverList<ash::MediaNotificationProviderObserver> observers_;

  MediaNotificationService* service_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
