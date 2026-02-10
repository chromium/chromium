// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"

#include "base/notimplemented.h"

StatusIconChromeOS::StatusIconChromeOS() = default;

StatusIconChromeOS::~StatusIconChromeOS() = default;

void StatusIconChromeOS::OnClick() {
  DispatchClickEvent();
}

void StatusIconChromeOS::SetImage(const gfx::ImageSkia& image) {
  // TODO(b:463428431): Implement this function.
  NOTIMPLEMENTED();
}

void StatusIconChromeOS::SetToolTip(const std::u16string& tool_tip) {
  // TODO(b:463428431): Implement this function.
  NOTIMPLEMENTED();
}

void StatusIconChromeOS::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const std::u16string& title,
    const std::u16string& contents,
    const message_center::NotifierId& notifier_id) {
  NOTIMPLEMENTED();
}

void StatusIconChromeOS::UpdatePlatformContextMenu(StatusIconMenuModel* model) {
  NOTIMPLEMENTED();
}
