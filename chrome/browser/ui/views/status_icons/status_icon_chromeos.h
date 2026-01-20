// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_

#include "chrome/browser/status_icons/status_icon.h"

class StatusIconChromeOS : public StatusIcon {
 public:
  StatusIconChromeOS();

  StatusIconChromeOS(const StatusIconChromeOS&) = delete;
  StatusIconChromeOS& operator=(const StatusIconChromeOS&) = delete;

  ~StatusIconChromeOS() override;

  // StatusIcon:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const std::u16string& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override;

  void OnClick();

 protected:
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
