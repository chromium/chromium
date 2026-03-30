// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace send_tab_to_self {

namespace {

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
  constexpr auto kPrimaryIconBorder = gfx::Insets(6);
  icon->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon;
}

}  // namespace

SendTabToSelfBubbleDeviceButton::SendTabToSelfBubbleDeviceButton(
    SendTabToSelfDevicePickerBubbleView* bubble,
    const TargetDeviceInfo& device_info)
    : HoverButton(base::BindRepeating(
                      &SendTabToSelfDevicePickerBubbleView::DeviceButtonPressed,
                      base::Unretained(bubble),
                      base::Unretained(this)),
                  CreateIcon(device_info.form_factor),
                  base::UTF8ToUTF16(device_info.device_name),
                  device_info.GetLastActiveTimeForDisplay()) {
  device_name_ = device_info.device_name;
  device_guid_ = device_info.cache_guid;
  device_form_factor_ = device_info.form_factor;
  SetEnabled(true);
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

BEGIN_METADATA(SendTabToSelfBubbleDeviceButton)
END_METADATA

}  // namespace send_tab_to_self
