// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_device_button.h"

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/grit/generated_resources.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/protocol/sync.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

namespace send_tab_to_self {

namespace {

enum class DeviceIconType {
  kDesktop = 0,
  kPhone = 1,
};

SkColor GetColorfromTheme() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  return native_theme->GetSystemColor(
      ui::NativeTheme::kColorId_DefaultIconColor);
}

gfx::ImageSkia CreateDeviceIcon(DeviceIconType icon_type) {
  const gfx::VectorIcon* vector_icon;
  switch (icon_type) {
    case DeviceIconType::kDesktop:
      vector_icon = &kHardwareComputerIcon;
      break;
    case DeviceIconType::kPhone:
      vector_icon = &kHardwareSmartphoneIcon;
      break;
  }

  constexpr int kPrimaryIconSize = 20;
  return gfx::CreateVectorIcon(*vector_icon, kPrimaryIconSize,
                               GetColorfromTheme());
}

std::unique_ptr<views::ImageView> CreateIconView(
    const sync_pb::SyncEnums::DeviceType device_type) {
  gfx::ImageSkia image = CreateDeviceIcon(
      device_type == sync_pb::SyncEnums::TYPE_PHONE ? DeviceIconType::kPhone
                                                    : DeviceIconType::kDesktop);
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(image);
  constexpr auto kPrimaryIconBorder = gfx::Insets(6);
  icon_view->SetBorder(views::CreateEmptyBorder(kPrimaryIconBorder));
  return icon_view;
}

base::string16 GetLastUpdatedTime(const TargetDeviceInfo& device_info) {
  int time_in_days =
      (base::Time::Now() - device_info.last_updated_timestamp).InDays();
  if (time_in_days == 0) {
    return l10n_util::GetStringUTF16(
        IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_TODAY_SEND_TAB_TO_SELF);
  } else if (time_in_days == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_DAY_SEND_TAB_TO_SELF,
        base::UTF8ToUTF16(std::to_string(time_in_days)));
  }
  return l10n_util::GetStringFUTF16(
      IDS_OMNIBOX_BUBBLE_ITEM_SUBTITLE_DAYS_SEND_TAB_TO_SELF,
      base::UTF8ToUTF16(std::to_string(time_in_days)));
}

}  // namespace

SendTabToSelfBubbleDeviceButton::SendTabToSelfBubbleDeviceButton(
    views::ButtonListener* button_listener,
    const TargetDeviceInfo& device_info,
    int button_tag)
    : HoverButton(button_listener,
                  CreateIconView(device_info.device_type),
                  base::UTF8ToUTF16(device_info.device_name),
                  GetLastUpdatedTime(device_info)) {
  device_name_ = device_info.device_name;
  device_guid_ = device_info.cache_guid;
  device_type_ = device_info.device_type;
  set_tag(button_tag);
  SetEnabled(true);
}

SendTabToSelfBubbleDeviceButton::~SendTabToSelfBubbleDeviceButton() = default;

}  // namespace send_tab_to_self
