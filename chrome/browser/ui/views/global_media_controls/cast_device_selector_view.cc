// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/cast_device_selector_view.h"

#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_device_entry_ui.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_helper.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace {

constexpr gfx::Size kPreferredSize{370, 0};
constexpr gfx::Size kCloseButtonSize{20, 20};

constexpr int kBackgroundBorderThickness = 1;
constexpr int kBackgroundCornerRadius = 8;
constexpr int kDeviceEntryCornerRadius = 4;
constexpr int kBackgroundSeparator = 8;
constexpr int kDeviceContainerSeparator = 4;
constexpr int kDeviceEntrySeparator = 8;
constexpr int kCloseButtonIconSize = 16;
constexpr int kDeviceEntryIconSize = 20;

constexpr gfx::Insets kBackgroundInsets = gfx::Insets::VH(16, 8);
constexpr gfx::Insets kCastToRowInsets = gfx::Insets::VH(0, 8);

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
  SetBorder(views::CreateThemedRoundedRectBorder(
      kBackgroundBorderThickness, kBackgroundCornerRadius,
      media_color_theme_.device_selector_border_color_id));
  SetBackground(views::CreateThemedRoundedRectBackground(
      media_color_theme_.device_selector_background_color_id,
      kBackgroundCornerRadius));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBackgroundInsets,
      kBackgroundSeparator));

  // |cast_to_row| holds the cast to label and the close button.
  auto* cast_to_row = AddChildView(std::make_unique<views::BoxLayoutView>());
  cast_to_row->SetInsideBorderInsets(kCastToRowInsets);

  // Create the cast to label.
  views::Label* cast_to_label =
      cast_to_row->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_GLOBAL_MEDIA_CONTROLS_CAST_TO_TEXT),
          views::style::CONTEXT_LABEL, views::style::STYLE_HEADLINE_5));
  cast_to_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  cast_to_row->SetFlexForView(cast_to_label, 1);

  // Create the close button.
  auto close_button =
      std::make_unique<global_media_controls::MediaActionButton>(
          base::BindRepeating(&CastDeviceSelectorView::HideDevices,
                              base::Unretained(this)),
          global_media_controls::kEmptyMediaActionButtonId,
          IDS_GLOBAL_MEDIA_CONTROLS_CLOSE_DEVICE_LIST_TEXT,
          kCloseButtonIconSize, vector_icons::kCloseIcon, kCloseButtonSize,
          media_color_theme_.secondary_foreground_color_id,
          media_color_theme_.secondary_foreground_color_id,
          media_color_theme_.focus_ring_color_id);
  close_button_ = cast_to_row->AddChildView(std::move(close_button));

  // Create the container view to hold available devices.
  device_container_view_ =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  device_container_view_->SetOrientation(
      views::BoxLayout::Orientation::kVertical);
  device_container_view_->SetBetweenChildSpacing(kDeviceContainerSeparator);

  if (show_devices) {
    ShowDevices();
  } else {
    UpdateVisibility();
  }
}

CastDeviceSelectorView::~CastDeviceSelectorView() = default;

///////////////////////////////////////////////////////////////////////////////
// global_media_controls::MediaItemUIDeviceSelector implementations:

void CastDeviceSelectorView::SetMediaItemUIUpdatedView(
    global_media_controls::MediaItemUIUpdatedView* view) {
  media_item_ui_updated_view_ = view;
}

void CastDeviceSelectorView::ShowDevices() {
  CHECK(!is_expanded_);
  is_expanded_ = true;
  UpdateVisibility();
}

void CastDeviceSelectorView::HideDevices() {
  CHECK(is_expanded_);
  is_expanded_ = false;
  UpdateVisibility();
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
    auto device_view = BuildCastDeviceEntryView(
        base::BindRepeating(&CastDeviceSelectorView::OnCastDeviceSelected,
                            base::Unretained(this), device->id),
        base::UTF8ToUTF16(device->name), device->icon);
    device_container_view_->AddChildView(std::move(device_view));
  }
  if (media_item_ui_updated_view_) {
    media_item_ui_updated_view_->UpdateDeviceSelectorAvailability(
        device_container_view_->children().size() > 0);
  }
  UpdateVisibility();
}

///////////////////////////////////////////////////////////////////////////////
// CastDeviceSelectorView implementations:

std::unique_ptr<HoverButton> CastDeviceSelectorView::BuildCastDeviceEntryView(
    views::Button::PressedCallback callback,
    const std::u16string& text,
    global_media_controls::mojom::IconType icon) {
  std::unique_ptr<HoverButton> device_entry_button;
  if (icon == global_media_controls::mojom::IconType::kThrobber) {
    device_entry_button = std::make_unique<HoverButton>(
        std::move(callback), media_router::CreateThrobber(), text);
  } else {
    device_entry_button = std::make_unique<HoverButton>(
        std::move(callback),
        ui::ImageModel::FromVectorIcon(
            GetVectorIcon(icon),
            media_color_theme_.secondary_foreground_color_id,
            kDeviceEntryIconSize),
        text);
  }
  device_entry_button->SetLabelStyle(views::style::STYLE_BODY_2);
  device_entry_button->SetEnabledTextColorIds(
      media_color_theme_.secondary_foreground_color_id);
  device_entry_button->SetImageLabelSpacing(kDeviceEntrySeparator);
  device_entry_button->SetFocusRingCornerRadius(kDeviceEntryCornerRadius);
  return device_entry_button;
}

void CastDeviceSelectorView::OnCastDeviceSelected(
    const std::string& device_id) {
  if (device_list_host_) {
    device_list_host_->SelectDevice(device_id);
  }
}

void CastDeviceSelectorView::UpdateVisibility() {
  // Show the view if user requests to show the list and there are also
  // available devices.
  SetVisible(is_expanded_ && (device_container_view_->children().size() > 0));

  // Visibility changes can result in size changes, which should change sizes of
  // parent views too.
  PreferredSizeChanged();

  // Update the casting state on the parent view.
  if (media_item_ui_updated_view_) {
    media_item_ui_updated_view_->UpdateDeviceSelectorVisibility(is_expanded_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Helper functions for testing:

global_media_controls::MediaActionButton*
CastDeviceSelectorView::GetCloseButtonForTesting() {
  return close_button_;
}

views::View* CastDeviceSelectorView::GetDeviceContainerViewForTesting() {
  return device_container_view_;
}

BEGIN_METADATA(CastDeviceSelectorView)
END_METADATA
