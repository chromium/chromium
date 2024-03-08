// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/cast_device_selector_view.h"

#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout_view.h"

namespace {

constexpr int kBackgroundCornerRadius = 16;

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::TLBR(16, 8, 8, 8);

constexpr gfx::Size kPreferredSize{350, 0};

}  // namespace

CastDeviceSelectorView::CastDeviceSelectorView(
    mojo::PendingRemote<global_media_controls::mojom::DeviceListHost>
        device_list_host,
    mojo::PendingReceiver<global_media_controls::mojom::DeviceListClient>
        device_list_client,
    media_message_center::MediaColorTheme media_color_theme,
    bool show_devices)
    : device_list_host_(std::move(device_list_host)),
      device_list_client_(this, std::move(device_list_client)),
      media_color_theme_(media_color_theme) {
  SetPreferredSize(kPreferredSize);
  SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme_.device_selector_background_color_id,
      kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets));

  device_container_view_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  device_container_view_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);

  SetVisible(false);
  if (show_devices) {
    ShowDevices();
  }
}

CastDeviceSelectorView::~CastDeviceSelectorView() = default;

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::MediaItemUIDeviceSelector implementations:

void CastDeviceSelectorView::SetMediaItemUIView(
    global_media_controls::MediaItemUIView* view) {
  media_item_ui_view_ = view;
}

void CastDeviceSelectorView::ShowDevices() {
  CHECK(!is_expanded_);
  is_expanded_ = true;

  SetVisible(is_expanded_);
  PreferredSizeChanged();
}

void CastDeviceSelectorView::HideDevices() {
  CHECK(is_expanded_);
  is_expanded_ = false;

  SetVisible(is_expanded_);
  PreferredSizeChanged();
}

bool CastDeviceSelectorView::IsDeviceSelectorExpanded() {
  return is_expanded_;
}

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::mojom::DeviceListClient implementations:

void CastDeviceSelectorView::OnDevicesUpdated(
    std::vector<global_media_controls::mojom::DevicePtr> devices) {
  device_container_view_->RemoveAllChildViews();
  for (const auto& device : devices) {
    auto device_view = std::make_unique<CastDeviceEntryViewAsh>(
        base::BindRepeating(&CastDeviceSelectorView::OnCastDeviceSelected,
                            base::Unretained(this), device->id),
        media_color_theme_.primary_foreground_color_id,
        media_color_theme_.secondary_foreground_color_id, device);
    device_container_view_->AddChildView(std::move(device_view));
  }
  SetVisible(is_expanded_);
  PreferredSizeChanged();

  if (media_item_ui_view_) {
    media_item_ui_view_->OnDeviceSelectorViewDevicesChanged(
        device_container_view_->children().size() > 0);
  }
}

///////////////////////////////////////////////////////////////////////////////
// CastDeviceSelectorView implementations:

void CastDeviceSelectorView::OnCastDeviceSelected(
    const std::string& device_id) {
  if (device_list_host_) {
    device_list_host_->SelectDevice(device_id);
  }
}

BEGIN_METADATA(CastDeviceSelectorView)
END_METADATA
