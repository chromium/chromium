// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/global_media_controls/media_notification_provider_impl.h"

#include "ash/shell.h"
#include "ash/system/cast/media_cast_audio_selector_view.h"
#include "ash/system/media/media_color_theme.h"
#include "ash/system/media/media_notification_provider.h"
#include "ash/system/media/media_notification_provider_observer.h"
#include "ash/system/media/media_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service.h"
#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service_factory.h"
#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_producer.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/views/media_item_ui_detailed_view.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/cpp/media_session_service.h"
#include "ui/views/view.h"

namespace ash {

MediaNotificationProviderImpl::MediaNotificationProviderImpl(
    media_session::MediaSessionService* service)
    : item_manager_(global_media_controls::MediaItemManager::Create()) {
  CHECK_EQ(nullptr, MediaNotificationProvider::Get());
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
          item_manager_.get(), /*source_id=*/std::nullopt);
  item_manager_->AddItemProducer(media_session_item_producer_.get());

  media_color_theme_ = GetCrosMediaColorTheme();
}

MediaNotificationProviderImpl::~MediaNotificationProviderImpl() {
  CHECK_EQ(this, MediaNotificationProvider::Get());
  MediaNotificationProvider::Set(nullptr);

  RemoveMediaItemManagerFromCastService(item_manager_.get());
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
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    const std::string& show_devices_for_item_id) {
  CHECK(item_manager_);
  CHECK(color_theme_);
  auto media_item_ui_list_view =
      std::make_unique<global_media_controls::MediaItemUIListView>(
          global_media_controls::MediaItemUIListView::SeparatorStyle(
              color_theme_->separator_color, separator_thickness),
          should_clip_height);
  media_item_ui_list_view_ = media_item_ui_list_view->GetWeakPtr();
  entry_point_ = entry_point;
  show_devices_for_item_id_ = show_devices_for_item_id;
  item_manager_->SetDialogDelegate(this);
  base::UmaHistogramEnumeration("Media.GlobalMediaControls.EntryPoint",
                                entry_point_);
  return media_item_ui_list_view;
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
  // Since the user profile is now active, we can create a
  // CastMediaNotificationProducer for the MediaItemManager to access Cast media
  // items.
  cast_service_ =
      CastMediaNotificationProducerKeyedServiceFactory::GetForProfile(
          GetProfile());
  AddMediaItemManagerToCastService(item_manager_.get());

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

void MediaNotificationProviderImpl::AddMediaItemManagerToCastService(
    global_media_controls::MediaItemManager* media_item_manager) {
  // Cast service will not be created in tests.
  if (cast_service_) {
    cast_service_->AddMediaItemManager(media_item_manager);
  }
}

void MediaNotificationProviderImpl::RemoveMediaItemManagerFromCastService(
    global_media_controls::MediaItemManager* media_item_manager) {
  if (cast_service_) {
    cast_service_->RemoveMediaItemManager(media_item_manager);
  }
}

std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
MediaNotificationProviderImpl::BuildDeviceSelectorView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item,
    global_media_controls::GlobalMediaControlsEntryPoint entry_point,
    bool show_devices) {
  // Returns the Ash `MediaCastAudioSelectorView` if BackgroundListening feature
  // is enabled.
  if (base::FeatureList::IsEnabled(media::kBackgroundListening)) {
    auto* const profile = GetProfile();
    auto* const device_service = GetDeviceService(item);
    if (!ShouldShowDeviceSelectorView(profile, device_service, id, item,
                                      &device_selector_delegate_)) {
      return nullptr;
    }

    auto device_set = CreateHostAndClient(profile, id, item, device_service);

    return std::make_unique<MediaCastAudioSelectorView>(
        std::move(device_set.host), std::move(device_set.client),
        GetStopCastingCallback(profile, id, item), show_devices);
  }

  return BuildDeviceSelector(id, item, GetDeviceService(item),
                             &device_selector_delegate_, GetProfile(),
                             entry_point, show_devices, media_color_theme_);
}

std::unique_ptr<global_media_controls::MediaItemUIFooter>
MediaNotificationProviderImpl::BuildFooterView(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  return BuildFooter(id, item, GetProfile(), media_color_theme_);
}

global_media_controls::MediaItemUI*
MediaNotificationProviderImpl::ShowMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!media_item_ui_list_view_) {
    return nullptr;
  }

  bool show_devices =
      (!show_devices_for_item_id_.empty() && (id == show_devices_for_item_id_));
  auto media_display_page =
      (MediaTray::IsPinnedToShelf() ? global_media_controls::MediaDisplayPage::
                                          kSystemShelfMediaDetailedView
                                    : global_media_controls::MediaDisplayPage::
                                          kQuickSettingsMediaDetailedView);
  auto item_ui = std::make_unique<global_media_controls::MediaItemUIView>(
      id, item, BuildFooterView(id, item),
      BuildDeviceSelectorView(id, item, entry_point_, show_devices),
      color_theme_, media_color_theme_, media_display_page);
  auto* item_ui_ptr = item_ui.get();
  item_ui_observer_set_.Observe(id, item_ui_ptr);

  media_item_ui_list_view_->ShowItem(id, std::move(item_ui));
  for (auto& observer : observers_) {
    observer.OnNotificationListViewSizeChanged();
  }
  return item_ui_ptr;
}

void MediaNotificationProviderImpl::HideMediaItem(const std::string& id) {
  if (!media_item_ui_list_view_) {
    return;
  }
  media_item_ui_list_view_->HideItem(id);
  for (auto& observer : observers_) {
    observer.OnNotificationListViewSizeChanged();
  }
}

void MediaNotificationProviderImpl::RefreshMediaItem(
    const std::string& id,
    base::WeakPtr<media_message_center::MediaNotificationItem> item) {
  if (!media_item_ui_list_view_) {
    return;
  }

  bool show_devices =
      (!show_devices_for_item_id_.empty() && (id == show_devices_for_item_id_));
  auto* media_item_ui = media_item_ui_list_view_->GetItem(id);
  media_item_ui->UpdateFooterView(BuildFooterView(id, item));
  media_item_ui->UpdateDeviceSelector(
      BuildDeviceSelectorView(id, item, entry_point_, show_devices));

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
