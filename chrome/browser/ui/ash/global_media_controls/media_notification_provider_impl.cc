// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/media_notification_provider_impl.h"

#include "ash/shell.h"
#include "ash/system/media/media_color_theme.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/media_notification_provider_observer.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"
#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_producer.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_legacy_cast_footer_view.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "ui/views/view.h"

namespace ash {

namespace {

std::unique_ptr<global_media_controls::MediaItemUIFooter> BuildFooterView(
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    Profile* profile,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point) {
  if (item->SourceType() != media_message_center::SourceType::kCast ||
      !media_router::GlobalMediaControlsCastStartStopEnabled(profile)) {
    return nullptr;
  }
  // Show a stop button for the Cast item.
  return std::make_unique<MediaItemUILegacyCastFooterView>(base::BindRepeating(
      &CastMediaNotificationItem::StopCasting,
      static_cast<CastMediaNotificationItem*>(item.get())->GetWeakPtr(),
      entry_point));
}

}  // namespace

MediaNotificationProviderImpl::MediaNotificationProviderImpl(
    media_session::MediaSessionService* service)
    : item_manager_(global_media_controls::MediaItemManager::Create()) {
  DCHECK_EQ(nullptr, MediaNotificationProvider::Get());
  MediaNotificationProvider::Set(this);

  item_manager_->AddObserver(this);

  if (!service) {
    return;
  }
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;

  // Connect to receive audio focus events.
  service->BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  service->BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());

  media_session_item_producer_ =
      std::make_unique<global_media_controls::MediaSessionItemProducer>(
          std::move(audio_focus_remote), std::move(controller_manager_remote),
          item_manager_.get(), /*source_id=*/absl::nullopt);
  item_manager_->AddItemProducer(media_session_item_producer_.get());
}

MediaNotificationProviderImpl::~MediaNotificationProviderImpl() {
  DCHECK_EQ(this, MediaNotificationProvider::Get());
  MediaNotificationProvider::Set(nullptr);

  item_manager_->RemoveObserver(this);
  if (crosapi::CrosapiManager::IsInitialized()) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->media_ui_ash()
        ->RemoveObserver(this);
  }
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
  if (!item_manager_) {
    return false;
  }
  return item_manager_->HasActiveItems();
}

bool MediaNotificationProviderImpl::HasFrozenNotifications() {
  if (!item_manager_) {
    return false;
  }
  return item_manager_->HasFrozenItems();
}

std::unique_ptr<views::View>
MediaNotificationProviderImpl::GetMediaNotificationListView(
    int separator_thickness,
    bool should_clip_height,
    const std::string& item_id) {
  DCHECK(item_manager_);
  DCHECK(color_theme_);
  auto notification_list_view =
      std::make_unique<global_media_controls::MediaItemUIListView>(
          global_media_controls::MediaItemUIListView::SeparatorStyle(
              color_theme_->separator_color, separator_thickness),
          should_clip_height);
  active_session_view_ = notification_list_view->GetWeakPtr();
  if (item_id.empty()) {
    entry_point_ =
        global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray;
    item_manager_->SetDialogDelegate(this);
  } else {
    entry_point_ =
        global_media_controls::GlobalMediaControlsEntryPoint::kPresentation;
    item_manager_->SetDialogDelegateForId(this, item_id);
  }
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.EntryPoint",
                                entry_point_);
  return notification_list_view;
}

void MediaNotificationProviderImpl::OnBubbleClosing() {
  item_manager_->SetDialogDelegate(nullptr);
}

void MediaNotificationProviderImpl::SetColorTheme(
    const media_message_center::NotificationTheme& color_theme) {
  color_theme_ = color_theme;
}

global_media_controls::MediaItemManager*
MediaNotificationProviderImpl::GetMediaItemManager() {
  return item_manager_.get();
}

void MediaNotificationProviderImpl::OnPrimaryUserSessionStarted() {
  if (!media_router::GlobalMediaControlsCastStartStopEnabled(GetProfile()) ||
      !crosapi::CrosapiManager::IsInitialized()) {
    return;
  }
  supplemental_device_picker_producer_ =
      std::make_unique<SupplementalDevicePickerProducer>(item_manager_.get());
  item_manager_->AddItemProducer(supplemental_device_picker_producer_.get());
  crosapi::MediaUIAsh* media_ui =
      crosapi::CrosapiManager::Get()->crosapi_ash()->media_ui_ash();
  media_ui->AddObserver(this);

  for (const auto& device_service : media_ui->device_services()) {
    device_service.second->SetDevicePickerProvider(
        supplemental_device_picker_producer_->PassRemote());
  }
}

global_media_controls::MediaItemUI*
MediaNotificationProviderImpl::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!active_session_view_) {
    return nullptr;
  }
  auto item_ui = std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, BuildFooterView(item, GetProfile(), entry_point_),
      BuildDeviceSelector(id, item, GetDeviceService(item),
                          &device_selector_delegate_, GetProfile(),
                          entry_point_),
      color_theme_, GetCrosMediaColorTheme(),
      media_message_center::MediaDisplayPage::kQuickSettingsMediaDetailedView);
  auto* item_ui_ptr = item_ui.get();
  item_ui_observer_set_.Observe(id, item_ui_ptr);

  active_session_view_->ShowItem(id, std::move(item_ui));
  for (auto& observer : observers_) {
    observer.OnNotificationListViewSizeChanged();
  }
  return item_ui_ptr;
}

void MediaNotificationProviderImpl::HideMediaItem(const std::string& id) {
  if (!active_session_view_) {
    return;
  }
  active_session_view_->HideItem(id);
  for (auto& observer : observers_) {
    observer.OnNotificationListViewSizeChanged();
  }
}

void MediaNotificationProviderImpl::HideMediaDialog() {
  ash::StatusAreaWidget::ForWindow(ash::Shell::Get()->GetPrimaryRootWindow())
      ->media_tray()
      ->CloseBubble();
}

void MediaNotificationProviderImpl::OnItemListChanged() {
  for (auto& observer : observers_) {
    observer.OnNotificationListChanged();
  }
}

void MediaNotificationProviderImpl::OnMediaItemUISizeChanged() {
  for (auto& observer : observers_) {
    observer.OnNotificationListViewSizeChanged();
  }
}

void MediaNotificationProviderImpl::OnMediaItemUIDestroyed(
    const std::string& id) {
  item_ui_observer_set_.StopObserving(id);
}

void MediaNotificationProviderImpl::OnDeviceServiceRegistered(
    global_media_controls::mojom::DeviceService* device_service) {
  device_service->SetDevicePickerProvider(
      supplemental_device_picker_producer_->PassRemote());
}

global_media_controls::mojom::DeviceService*
MediaNotificationProviderImpl::GetDeviceService(
    base::WeakPtr<media_message_center::MediaNotificationItem> item) const {
  if (!item || !item->GetSourceId()) {
    return nullptr;
  }
  if (device_service_for_testing_) {
    return device_service_for_testing_;
  }
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->media_ui_ash()
      ->GetDeviceService(*item->GetSourceId());
}

Profile* MediaNotificationProviderImpl::GetProfile() {
  return profile_for_testing_ ? profile_for_testing_.get()
                              : ProfileManager::GetActiveUserProfile();
}

}  // namespace ash
