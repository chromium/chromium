// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/media_notification_provider_impl.h"

#include "ash/public/cpp/media_notification_provider_observer.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/views/view.h"

MediaNotificationProviderImpl::MediaNotificationProviderImpl() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

MediaNotificationProviderImpl::~MediaNotificationProviderImpl() {
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);

  if (item_manager_)
    item_manager_->RemoveObserver(this);

  for (auto item_ui_pair : observed_item_uis_)
    item_ui_pair.second->RemoveObserver(this);
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
  if (!item_manager_)
    return false;
  return item_manager_->HasActiveItems();
}

bool MediaNotificationProviderImpl::HasFrozenNotifications() {
  if (!item_manager_)
    return false;
  return item_manager_->HasFrozenItems();
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetMediaNotificationListView(
    int separator_thickness) {
  DCHECK(item_manager_);
  DCHECK(color_theme_);
  auto notification_list_view =
      std::make_unique<global_media_controls::MediaItemUIListView>(
          global_media_controls::MediaItemUIListView::SeparatorStyle(
              color_theme_->separator_color, separator_thickness));
  active_session_view_ = notification_list_view.get();
  item_manager_->SetDialogDelegate(this);
  base::UmaHistogramEnumeration(
      "Media.GlobalMediaControls.EntryPoint",
      global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray);
  return std::move(notification_list_view);
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetActiveMediaNotificationView() {
  return std::make_unique<views::View>();
}

void MediaNotificationProviderImpl::OnBubbleClosing() {
  item_manager_->SetDialogDelegate(nullptr);
}

void MediaNotificationProviderImpl::SetColorTheme(
    const media_message_center::NotificationTheme& color_theme) {
  color_theme_ = color_theme;
}

global_media_controls::MediaItemUI*
MediaNotificationProviderImpl::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!active_session_view_)
    return nullptr;

  auto item_ui = std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, /*footer_view=*/nullptr, /*device_selector_view=*/nullptr,
      color_theme_);
  auto* item_ui_ptr = item_ui.get();
  item_ui_ptr->AddObserver(this);
  observed_item_uis_[id] = item_ui_ptr;

  active_session_view_->ShowItem(id, std::move(item_ui));
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();

  return item_ui_ptr;
}

void MediaNotificationProviderImpl::HideMediaItem(const std::string& id) {
  if (!active_session_view_)
    return;

  active_session_view_->HideItem(id);
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();
}

void MediaNotificationProviderImpl::OnItemListChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListChanged();
}

void MediaNotificationProviderImpl::OnMediaItemUISizeChanged() {
  for (auto& observer : observers_)
    observer.OnNotificationListViewSizeChanged();
}

void MediaNotificationProviderImpl::OnMediaItemUIDestroyed(
    const std::string& id) {
  auto iter = observed_item_uis_.find(id);
  DCHECK(iter != observed_item_uis_.end());

  iter->second->RemoveObserver(this);
  observed_item_uis_.erase(iter);
}

void MediaNotificationProviderImpl::OnUserProfileLoaded(
    const AccountId& account_id) {
  auto* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);

  if (user_manager::UserManager::Get()->GetPrimaryUser() == user) {
    service_ = MediaNotificationServiceFactory::GetForProfile(profile);
    item_manager_ = service_->media_item_manager();
    item_manager_->AddObserver(this);
  }
}
