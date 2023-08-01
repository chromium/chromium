// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/status_icons/status_tray_mac.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"

std::unique_ptr<StatusTray> StatusTray::Create() {
  return std::make_unique<StatusTrayMac>();
}

StatusTrayMac::StatusTrayMac() {
}

std::unique_ptr<StatusIcon> StatusTrayMac::CreatePlatformStatusIcon(
    StatusIconType type,
    const gfx::ImageSkia& image,
    const std::u16string& tool_tip) {
  auto icon = std::make_unique<StatusIconMac>();
  icon->SetImage(image);
  icon->SetToolTip(tool_tip);
  return std::move(icon);
}
