// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_SELECTOR_VIEW_H_

#include "chrome/browser/ui/views/controls/hover_button.h"
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

// Special hover button that displays a device with issue text. We create a
// subclass because the hover button lacks functionalities to customize the
// labels.
class IssueHoverButton : public HoverButton {
  METADATA_HEADER(IssueHoverButton, HoverButton)

 public:
  IssueHoverButton(PressedCallback callback,
                   global_media_controls::mojom::IconType icon,
                   const std::u16string& device_name,
                   const std::u16string& status_text,
                   ui::ColorId device_name_color_id,
                   ui::ColorId status_text_color_id);

  // HoverButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  views::Label* device_name_label() { return device_name_label_; }
  views::Label* status_text_label() { return status_text_label_; }

 private:
  raw_ptr<views::Label> device_name_label_ = nullptr;
  raw_ptr<views::Label> status_text_label_ = nullptr;
};

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
  void OnPermissionRejected() override;

  // Helper functions for testing:
  bool GetHasDeviceIssueForTesting();
  global_media_controls::MediaActionButton* GetCloseButtonForTesting();
  views::View* GetDeviceContainerViewForTesting();
  views::View* GetPermissionRejectedViewForTesting();

 private:
  // Build a device entry view for the given device information.
  std::unique_ptr<HoverButton> BuildCastDeviceEntryView(
      views::Button::PressedCallback callback,
      global_media_controls::mojom::IconType icon,
      const std::u16string& device_name,
      const std::u16string& status_text);

  // Callback for when a device is selected by user.
  void OnCastDeviceSelected(const std::string& device_id);

  // Update the visibility of the whole view which changes its size too.
  void UpdateVisibility();

  // Callback for when the close button is pressed.
  void CloseButtonPressed();

  // Returns true if there are available devices or
  // `has_permission_rejected_issue_` is True.
  bool IsDeviceSelectorAvailable();

  // Records whether the device list is expanded.
  bool is_expanded_ = false;

  // Records whether any of the available devices has an issue to be displayed.
  bool has_device_issue_ = false;

  // True if the local discovery permission has been rejected.
  bool has_permission_rejected_issue_ = false;

  raw_ptr<global_media_controls::MediaItemUIUpdatedView>
      media_item_ui_updated_view_ = nullptr;

  raw_ptr<global_media_controls::MediaActionButton> close_button_ = nullptr;
  raw_ptr<views::BoxLayoutView> device_container_view_ = nullptr;
  raw_ptr<views::BoxLayoutView> permission_rejected_view_ = nullptr;

  mojo::Remote<global_media_controls::mojom::DeviceListHost> device_list_host_;
  mojo::Receiver<global_media_controls::mojom::DeviceListClient>
      device_list_client_;
  media_message_center::MediaColorTheme media_color_theme_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_MEDIA_CONTROLS_CAST_DEVICE_SELECTOR_VIEW_H_
