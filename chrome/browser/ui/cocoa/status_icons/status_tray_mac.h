// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_TRAY_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_TRAY_MAC_H_

#include "chrome/browser/status_icons/status_tray.h"

class StatusTrayMac : public StatusTray {
 public:
  StatusTrayMac();

  StatusTrayMac(const StatusTrayMac&) = delete;
  StatusTrayMac& operator=(const StatusTrayMac&) = delete;

 protected:
  // Factory method for creating a status icon.
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override;
};

#endif  // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_TRAY_MAC_H_
