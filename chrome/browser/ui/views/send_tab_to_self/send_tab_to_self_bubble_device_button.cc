// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/remote_device_info.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

// Size of the checkmark icon.
constexpr int kCheckmarkIconSize = 20;

std::unique_ptr<views::ImageView> CreateIcon(
    const syncer::DeviceInfo::FormFactor device_form_factor) {
  static constexpr int kPrimaryIconSize = 20;
  return std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      GetDeviceIcon(device_form_factor), ui::kColorIcon, kPrimaryIconSize));
}

views::Button::PressedCallback GetPressedCallback(
    SendTabToSelfDevicePickerBubbleView* bubble,
    SendTabToSelfBubbleDeviceButton* button) {
  return base::BindRepeating(
      &SendTabToSelfDevicePickerBubbleView::SelectTargetDevice,
      base::Unretained(bubble), base::Unretained(button));
}

std::unique_ptr<views::ImageView> CreateCheckmarkIcon() {
  return std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
      ui::kColorAccent, kCheckmarkIconSize));
}

}  // namespace

SendTabToSelfBubbleDeviceButton::SendTabToSelfBubbleDeviceButton(
    SendTabToSelfDevicePickerBubbleView* bubble,
    const syncer::RemoteDeviceInfo& device_info)
    : HoverButton(GetPressedCallback(bubble, this),
                  CreateIcon(device_info.form_factor),
                  base::UTF8ToUTF16(device_info.device_name),
                  device_info.GetLastActiveTimeForDisplay(),
                  CreateCheckmarkIcon()) {
  device_name_ = device_info.device_name;
  device_guid_ = device_info.cache_guid;
  SetEnabled(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kListBoxOption);
  ApplyDeviceSelectionStyling();
  SetSelected(false);
}

void SendTabToSelfBubbleDeviceButton::ApplyDeviceSelectionStyling() {
  // Align content with dialog layout but keep hover backdrop full-width.
  auto* provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets = provider->GetInsetsMetric(views::INSETS_DIALOG);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, dialog_insets.left(), 0, dialog_insets.right())));
  if (title()) {
    title()->SetTextStyle(views::style::STYLE_BODY_5_MEDIUM);
    title()->SetEnabledColor(ui::kColorLabelForeground);
  }
  if (subtitle()) {
    subtitle()->SetTextStyle(views::style::STYLE_CAPTION);
    subtitle()->SetEnabledColor(ui::kColorLabelForegroundSecondary);
    // Add a small margin to separate the subtitle from the device name.
    subtitle()->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(2, 0, 0, 0));
  }
}

void SendTabToSelfBubbleDeviceButton::SetSelected(bool selected) {
  is_selected_ = selected;
  // In practice, `secondary_view()` is the checkmark icon (created by
  // `CreateCheckmarkIcon()`) that indicates the device is selected.
  if (secondary_view()) {
    secondary_view()->SetVisible(selected);
  }
  GetViewAccessibility().SetIsSelected(selected);
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

BEGIN_METADATA(SendTabToSelfBubbleDeviceButton)
END_METADATA

}  // namespace send_tab_to_self
