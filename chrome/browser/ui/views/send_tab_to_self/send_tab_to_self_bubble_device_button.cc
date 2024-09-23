// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_device_picker_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
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

std::u16string GetLastUpdatedTime(const TargetDeviceInfo& device_info) {
  int time_in_days =
      (base::Time::Now() - device_info.last_updated_timestamp).InDays();
  if (time_in_days == 0) {
    return l10n_util::GetStringUTF16(
        IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_TODAY_SEND_TAB_TO_SELF);
  } else if (time_in_days == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_DAY_SEND_TAB_TO_SELF,
        base::UTF8ToUTF16(base::NumberToString(time_in_days)));
  }
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_DAYS_SEND_TAB_TO_SELF,
      base::UTF8ToUTF16(base::NumberToString(time_in_days)));
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
                  GetLastUpdatedTime(device_info)) {
  device_name_ = device_info.device_name;
  device_guid_ = device_info.cache_guid;
  device_form_factor_ = device_info.form_factor;
  SetEnabled(true);
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

BEGIN_METADATA(SendTabToSelfBubbleDeviceButton)
END_METADATA

}  // namespace send_tab_to_self
