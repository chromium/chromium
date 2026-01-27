// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_chromeos.h"

#include <memory>

#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"
#include "chrome/common/chrome_features.h"

StatusTrayChromeOS::StatusTrayChromeOS() = default;

StatusTrayChromeOS::~StatusTrayChromeOS() = default;

std::unique_ptr<StatusIcon> StatusTrayChromeOS::CreatePlatformStatusIcon(
    StatusIconType type,
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip) {
  // TODO(b:463428431): Calculate correct icon id..
  int64_t next_icon_id = 0;

  auto icon = std::make_unique<StatusIconChromeOS>(next_icon_id);
  icon->SetImage(image);
  icon->SetToolTip(tool_tip);
  icon->Initialize();

  return icon;
}

std::unique_ptr<StatusTray> StatusTray::Create() {
  // TODO(b:463428431): Return StatusTrayChromeOS when ready.
  return nullptr;
}
