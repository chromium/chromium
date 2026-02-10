// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_chromeos.h"

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"

using TrayIconConfiguration = ash::TrayIconConfiguration;

StatusIconChromeOS::StatusIconChromeOS(int64_t icon_id) : id_(icon_id) {
  display_observer_.emplace(this);
}

StatusIconChromeOS::~StatusIconChromeOS() {
  TrayIconConfiguration icon_config;
  icon_config.id = id_;

  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    ash::Shell::Get()->RemoveStatusTrayIcon(icon_config, display.id());
  }
}

void StatusIconChromeOS::Initialize() {
  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    AddStatusIconForDisplay(display.id());
  }

  initialized_ = true;
}

void StatusIconChromeOS::OnClick() {
  DispatchClickEvent();
}

void StatusIconChromeOS::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
  if (!initialized_) {
    return;
  }

  TrayIconConfiguration icon_config;
  icon_config.id = id_;
  icon_config.image = image_;

  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    ash::Shell::Get()->UpdateStatusTrayIcon(icon_config, display.id());
  }
}

void StatusIconChromeOS::SetToolTip(const std::u16string& tool_tip) {
  tool_tip_ = tool_tip;
  if (!initialized_) {
    return;
  }

  TrayIconConfiguration icon_config;
  icon_config.id = id_;
  icon_config.tool_tip = tool_tip_;

  for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
    ash::Shell::Get()->UpdateStatusTrayIcon(icon_config, display.id());
  }
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

void StatusIconChromeOS::OnDisplayAdded(const display::Display& new_display) {
  AddStatusIconForDisplay(new_display.id());
}

void StatusIconChromeOS::OnWillRemoveDisplays(
    const display::Displays& removed_displays) {
  TrayIconConfiguration icon_config;
  icon_config.id = id_;

  for (const auto& display : removed_displays) {
    ash::Shell::Get()->RemoveStatusTrayIcon(icon_config, display.id());
  }
}

void StatusIconChromeOS::AddStatusIconForDisplay(int64_t display_id) {
  TrayIconConfiguration icon_config;
  icon_config.id = id_;
  icon_config.tool_tip = tool_tip_;
  icon_config.image = image_;

  ash::Shell::Get()->AddStatusTrayIcon(
      icon_config, display_id,
      base::BindRepeating(&StatusIconChromeOS::OnClick,
                          base::Unretained(this)));
}
