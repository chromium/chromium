// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace send_tab_to_self {

namespace {

// Size of the checkmark icon.
constexpr int kCheckmarkIconSize = 20;

const gfx::VectorIcon& GetIconType(
    const syncer::DeviceInfo::FormFactor& device_form_factor) {
  switch (device_form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      return kHardwareSmartphoneIcon;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return kTabletIcon;
    default:
      return kHardwareComputerIcon;
  }
}

std::unique_ptr<views::ImageView> CreateIcon(
    const syncer::DeviceInfo::FormFactor device_form_factor) {
  static constexpr int kPrimaryIconSize = 20;
  auto icon = std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      GetIconType(device_form_factor), ui::kColorIcon, kPrimaryIconSize));
  if (!base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)) {
    constexpr auto kPrimaryIconBorder = gfx::Insets(6);
    icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  }
  return icon;
}

views::Button::PressedCallback GetPressedCallback(
    SendTabToSelfDevicePickerBubbleView* bubble,
    SendTabToSelfBubbleDeviceButton* button) {
  if (base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)) {
    return base::BindRepeating(
        &SendTabToSelfDevicePickerBubbleView::SelectTargetDevice,
        base::Unretained(bubble), base::Unretained(button));
  }
  return base::BindRepeating(
      &SendTabToSelfDevicePickerBubbleView::DeviceButtonPressed,
      base::Unretained(bubble), base::Unretained(button));
}

std::unique_ptr<views::ImageView> CreateCheckmarkIcon() {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)) {
    return nullptr;
  }
  return std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
      kCheckIcon, ui::kColorAccent, kCheckmarkIconSize));
}

}  // namespace

SendTabToSelfBubbleDeviceButton::SendTabToSelfBubbleDeviceButton(
    SendTabToSelfDevicePickerBubbleView* bubble,
    const TargetDeviceInfo& device_info)
    : HoverButton(GetPressedCallback(bubble, this),
                  CreateIcon(device_info.form_factor),
                  base::UTF8ToUTF16(device_info.device_name),
                  device_info.GetLastActiveTimeForDisplay(),
                  CreateCheckmarkIcon()) {
  device_name_ = device_info.device_name;
  device_guid_ = device_info.cache_guid;
  device_form_factor_ = device_info.form_factor;
  SetEnabled(true);
  SetSelected(false);
  if (base::FeatureList::IsEnabled(kSendTabToSelfShowTargetsInContextMenus)) {
    ApplyDeviceSelectionStyling();
  }
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
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

BEGIN_METADATA(SendTabToSelfBubbleDeviceButton)
END_METADATA

}  // namespace send_tab_to_self
