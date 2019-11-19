// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_tray_linux.h"

#include <memory>

#include "build/build_config.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

StatusTrayLinux::StatusTrayLinux() {
}

StatusTrayLinux::~StatusTrayLinux() {
}

std::unique_ptr<StatusIcon> StatusTrayLinux::CreatePlatformStatusIcon(
    StatusIconType type,
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) {
  return StatusIconLinuxWrapper::CreateWrappedStatusIcon(image, tool_tip);
}

std::unique_ptr<StatusTray> StatusTray::Create() {
  return std::make_unique<StatusTrayLinux>();
}
#else  // defined(OS_CHROMEOS)
std::unique_ptr<StatusTray> StatusTray::Create() {
  return nullptr;
}
#endif
