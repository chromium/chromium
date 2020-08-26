// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_notification_provider_impl.h"

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/views/view.h"

MediaNotificationProviderImpl::MediaNotificationProviderImpl() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

MediaNotificationProviderImpl::~MediaNotificationProviderImpl() {
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);

  if (service_)
    service_->RemoveObserver(this);
}

void MediaNotificationProviderImpl::AddObserver(
    ash::MediaNotificationProviderObserver* observer) {
  observers_.AddObserver(observer);
}

void MediaNotificationProviderImpl::RemoveObserver(
    ash::MediaNotificationProviderObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool MediaNotificationProviderImpl::HasActiveNotifications() {
  if (!service_)
    return false;
  return service_->HasActiveNotifications();
}

bool MediaNotificationProviderImpl::HasFrozenNotifications() {
  if (!service_)
    return false;
  return service_->HasFrozenNotifications();
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetMediaNotificationListView() {
  return std::make_unique<views::View>();
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetActiveMediaNotificationView() {
  return std::make_unique<views::View>();
}

void MediaNotificationProviderImpl::OnNotificationListChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationProviderImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);

  if (user_manager::UserManager::Get()->GetPrimaryUser() == user) {
    service_ = MediaNotificationServiceFactory::GetForProfile(profile);
    service_->AddObserver(this);
  }
}
