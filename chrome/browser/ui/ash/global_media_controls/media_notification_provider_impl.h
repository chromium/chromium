// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
#define CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/system/media/media_notification_provider.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/ui/ash/global_media_controls/media_item_ui_device_selector_delegate_ash.h"
#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_producer.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/media_message_center/media_notification_view_impl.h"

class CastMediaNotificationProducerKeyedService;
class Profile;

namespace global_media_controls {
namespace mojom {
class DeviceService;
}  // namespace mojom
class MediaItemManager;
class MediaItemUIDeviceSelector;
class MediaItemUIFooter;
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
      public global_media_controls::MediaItemUIObserver,
      public crosapi::MediaUIAsh::Observer {
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
      bool should_clip_height,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      const std::string& show_devices_for_item_id) override;
  void OnBubbleClosing() override;
  void SetColorTheme(
      const media_message_center::NotificationTheme& color_theme) override;
  global_media_controls::MediaItemManager* GetMediaItemManager() override;
  void OnPrimaryUserSessionStarted() override;
  void AddMediaItemManagerToCastService(
      global_media_controls::MediaItemManager* media_item_manager) override;
  void RemoveMediaItemManagerFromCastService(
      global_media_controls::MediaItemManager* media_item_manager) override;
  std::unique_ptr<global_media_controls::MediaItemUIDeviceSelector>
  BuildDeviceSelectorView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item,
      global_media_controls::GlobalMediaControlsEntryPoint entry_point,
      bool show_devices) override;
  std::unique_ptr<global_media_controls::MediaItemUIFooter> BuildFooterView(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;

  // global_media_controls::MediaDialogDelegate:
  global_media_controls::MediaItemUI* ShowMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaItem(const std::string& id) override;
  void RefreshMediaItem(
      const std::string& id,
      base::WeakPtr<media_message_center::MediaNotificationItem> item) override;
  void HideMediaDialog() override;
  void Focus() override {}

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() override;
  void OnMediaDialogOpened() override {}
  void OnMediaDialogClosed() override {}

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUISizeChanged() override;

  // crosapi::MediaUIAsh::Observer:
  void OnDeviceServiceRegistered(
      global_media_controls::mojom::DeviceService* device_service) override;

  global_media_controls::MediaSessionItemProducer*
  media_session_item_producer_for_testing() {
    return media_session_item_producer_.get();
  }

  void set_profile_for_testing(Profile* profile) {
    profile_for_testing_ = profile;
  }

  void set_device_service_for_testing(
      global_media_controls::mojom::DeviceService* device_service) {
    device_service_for_testing_ = device_service;
  }

 private:
  Profile* GetProfile();

  global_media_controls::mojom::DeviceService* GetDeviceService(
      base::WeakPtr<media_message_center::MediaNotificationItem> item) const;

  base::ObserverList<MediaNotificationProviderObserver> observers_;

  base::WeakPtr<global_media_controls::MediaItemUIListView>
      media_item_ui_list_view_;

  std::string show_devices_for_item_id_;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;
  std::unique_ptr<SupplementalDevicePickerProducer>
      supplemental_device_picker_producer_;

  std::optional<media_message_center::NotificationTheme> color_theme_;

  std::optional<media_message_center::MediaColorTheme> media_color_theme_;

  global_media_controls::MediaItemUIObserverSet item_ui_observer_set_{this};

  MediaItemUIDeviceSelectorDelegateAsh device_selector_delegate_;

  global_media_controls::GlobalMediaControlsEntryPoint entry_point_{
      global_media_controls::GlobalMediaControlsEntryPoint::kSystemTray};

  raw_ptr<CastMediaNotificationProducerKeyedService> cast_service_ = nullptr;

  raw_ptr<Profile, DanglingUntriaged> profile_for_testing_ = nullptr;
  raw_ptr<global_media_controls::mojom::DeviceService>
      device_service_for_testing_ = nullptr;

  base::WeakPtrFactory<MediaNotificationProviderImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_PROVIDER_IMPL_H_
