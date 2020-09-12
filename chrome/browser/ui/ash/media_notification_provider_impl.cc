// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_notification_provider_impl.h"

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"
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

  for (auto containers_pair : observed_containers_)
    containers_pair.second->RemoveObserver(this);
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
MediaNotificationProviderImpl::GetMediaNotificationListView(
    SkColor separator_color,
    int separator_thickness) {
  DCHECK(service_);
  auto notification_list_view = std::make_unique<MediaNotificationListView>(
      MediaNotificationListView::SeparatorStyle(separator_color,
                                                separator_thickness));
  active_session_view_ = notification_list_view.get();
  service_->SetDialogDelegate(this);
  return std::move(notification_list_view);
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetActiveMediaNotificationView() {
  return std::make_unique<views::View>();
}

void MediaNotificationProviderImpl::OnBubbleClosing() {
  service_->SetDialogDelegate(nullptr);
}

MediaNotificationContainerImpl* MediaNotificationProviderImpl::ShowMediaSession(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!active_session_view_)
    return nullptr;

  auto container = std::make_unique<MediaNotificationContainerImplView>(
      id, item, service_,
      media_message_center::MediaNotificationViewImpl::BackgroundStyle::
          kAshStyle);
  MediaNotificationContainerImplView* container_ptr = container.get();
  container_ptr->AddObserver(this);
  observed_containers_[id] = container_ptr;

  active_session_view_->ShowNotification(id, std::move(container));
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();

  return container_ptr;
}

void MediaNotificationProviderImpl::HideMediaSession(const std::string& id) {
  if (!active_session_view_)
    return;

  active_session_view_->HideNotification(id);
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();
}

std::unique_ptr<OverlayMediaNotification> MediaNotificationProviderImpl::PopOut(
    const std::string& id,
    gfx::Rect bounds) {
  return active_session_view_->PopOut(id, bounds);
}

void MediaNotificationProviderImpl::OnNotificationListChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationProviderImpl::OnContainerSizeChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();
}

void MediaNotificationProviderImpl::OnContainerDestroyed(
    const std::string& id) {
  auto iter = observed_containers_.find(id);
  DCHECK(iter != observed_containers_.end());

  iter->second->RemoveObserver(this);
  observed_containers_.erase(iter);
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
