// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_DRIVE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_DRIVE_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// The WebUI controller for chrome::drive-internals, that is used for
// diagnosing issues of Drive on Chrome OS.
class DriveInternalsUI : public content::WebUIController {
 public:
  explicit DriveInternalsUI(content::WebUI* web_ui);

  DriveInternalsUI(const DriveInternalsUI&) = delete;
  DriveInternalsUI& operator=(const DriveInternalsUI&) = delete;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_DRIVE_INTERNALS_UI_H_
