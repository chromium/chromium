// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_CHROMEOS_H_

#include <cstdint>

#include "chrome/browser/status_icons/status_tray.h"

class StatusTrayChromeOS : public StatusTray {
 public:
  StatusTrayChromeOS();

  StatusTrayChromeOS(const StatusTrayChromeOS&) = delete;
  StatusTrayChromeOS& operator=(const StatusTrayChromeOS&) = delete;

  ~StatusTrayChromeOS() override;

 protected:
  // StatusTray:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override;

 private:
  int64_t NextIconId();

  // The unique icon ID we will assign to the next icon.
  uint32_t next_icon_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_CHROMEOS_H_
