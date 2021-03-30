// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_LINUX_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/status_icons/status_tray.h"

class StatusTrayLinux : public StatusTray {
 public:
  StatusTrayLinux();
  ~StatusTrayLinux() override;

 protected:
  // Overriden from StatusTray:
  std::unique_ptr<StatusIcon> CreatePlatformStatusIcon(
      StatusIconType type,
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatusTrayLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_TRAY_LINUX_H_
