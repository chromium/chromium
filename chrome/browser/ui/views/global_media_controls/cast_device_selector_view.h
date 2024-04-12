// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_SELECTOR_VIEW_H_

#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/global_media_controls/public/views/media_action_button.h"
#include "components/global_media_controls/public/views/media_item_ui_device_selector.h"
#include "components/media_message_center/notification_theme.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace global_media_controls {
class MediaItemUIUpdatedView;
}  // namespace global_media_controls

namespace views {
class BoxLayoutView;
}  // namespace views

// CastDeviceSelectorView holds a list of devices available for casting the
// given media session. This is used within MediaDialogView on non-CrOS desktop
// platforms and replaces MediaItemUIDeviceSelectorView when the
// media::kGlobalMediaControlsUpdatedUI flag is enabled.
class CastDeviceSelectorView
    : public global_media_controls::MediaItemUIDeviceSelector,
      public global_media_controls::mojom::DeviceListClient {
  METADATA_HEADER(CastDeviceSelectorView,
                  global_media_controls::MediaItemUIDeviceSelector)

 public:
  CastDeviceSelectorView(
      mojo::PendingRemote<global_media_controls::mojom::DeviceListHost>
          device_list_host,
      mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
          device_list_client,
      media_message_center::MediaColorTheme media_color_theme,
      bool show_devices = false);
  ~CastDeviceSelectorView() override;

  // global_media_controls::MediaItemUIDeviceSelector:
  void SetMediaItemUIUpdatedView(
      global_media_controls::MediaItemUIUpdatedView* view) override;
  void OnColorsChanged(SkColor foreground_color,
                       SkColor background_color) override {}
  void UpdateCurrentAudioDevice(const std::string& current_device_id) override {
  }
  void ShowDevices() override;
  void HideDevices() override;
  bool IsDeviceSelectorExpanded() override;

  // global_media_controls::mojom::DeviceListClient:
  void OnDevicesUpdated(
      std::vector<global_media_controls::mojom::DevicePtr> devices) override;

  // Helper functions for testing:
  global_media_controls::MediaActionButton* GetCloseButtonForTesting();

 private:
  friend class CastDeviceSelectorViewTest;

  void OnCastDeviceSelected(const std::string& device_id);

  // Update the visibility of the whole view which changes its size too.
  void UpdateVisibility();

  bool is_expanded_ = false;

  raw_ptr<global_media_controls::MediaItemUIUpdatedView>
      media_item_ui_updated_view_ = nullptr;

  raw_ptr<global_media_controls::MediaActionButton> close_button_ = nullptr;
  raw_ptr<views::BoxLayoutView> device_container_view_ = nullptr;

  mojo::Remote<global_media_controls::mojom::DeviceListHost> device_list_host_;
  mojo::Receiver<global_media_controls::mojom::DeviceListClient>
      device_list_client_;
  media_message_center::MediaColorTheme media_color_theme_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_SELECTOR_VIEW_H_
