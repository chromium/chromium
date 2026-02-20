// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_chromeos.h"

#include <memory>

#include "ash/constants/chrome_custom_icon_catalog.h"
#include "base/notreached.h"
#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"

namespace {

using ChromeCustomIconCatalogName = ash::ChromeCustomIconCatalogName;

constexpr int64_t kBaseIconId = 2;
int64_t ReservedIconId(StatusTray::StatusIconType type) {
  return kBaseIconId + static_cast<int64_t>(type);
}

ChromeCustomIconCatalogName GetCatalogName(StatusTray::StatusIconType type) {
  switch (type) {
    case StatusTray::GLIC_ICON:
      return ChromeCustomIconCatalogName::kGlic;
    case StatusTray::NOTIFICATION_TRAY_ICON:
    case StatusTray::MEDIA_STREAM_CAPTURE_ICON:
    case StatusTray::BACKGROUND_MODE_ICON:
    case StatusTray::OTHER_ICON:
      return ChromeCustomIconCatalogName::kNotSupported;
    default:
      NOTREACHED();
  }
}

}  // namespace

StatusTrayChromeOS::StatusTrayChromeOS() = default;

StatusTrayChromeOS::~StatusTrayChromeOS() = default;

std::unique_ptr<StatusIcon> StatusTrayChromeOS::CreatePlatformStatusIcon(
    StatusIconType type,
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip) {
  // To add icons of other `StatusIconType` to StatusTray of ChromeOS, please
  // update a new entry in `ash::ChromeCustomIconCatalogName`.
  if (GetCatalogName(type) == ChromeCustomIconCatalogName::kNotSupported) {
    return nullptr;
  }

  int64_t next_icon_id;
  if (type == StatusTray::OTHER_ICON) {
    next_icon_id = NextIconId();
  } else {
    next_icon_id = ReservedIconId(type);
  }

  auto icon = std::make_unique<StatusIconChromeOS>(next_icon_id);
  icon->SetImage(image);
  icon->SetToolTip(tool_tip);
  icon->Initialize();

  return icon;
}

int64_t StatusTrayChromeOS::NextIconId() {
  int64_t icon_id = next_icon_id_++;
  return kBaseIconId +
         static_cast<int64_t>(StatusTray::NAMED_STATUS_ICON_COUNT) + icon_id;
}

std::unique_ptr<StatusTray> StatusTray::Create() {
  return std::make_unique<StatusTrayChromeOS>();
}
