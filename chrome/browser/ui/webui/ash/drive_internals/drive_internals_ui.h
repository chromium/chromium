// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_DRIVE_INTERNALS_DRIVE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_DRIVE_INTERNALS_DRIVE_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class DriveInternalsUI;

// WebUIConfig for chrome://drive-internals
class DriveInternalsUIConfig
    : public content::DefaultWebUIConfig<DriveInternalsUI> {
 public:
  DriveInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIDriveInternalsHost) {}
};

// The WebUI controller for chrome::drive-internals, that is used for
// diagnosing issues of Drive on Chrome OS.
class DriveInternalsUI : public content::WebUIController {
 public:
  explicit DriveInternalsUI(content::WebUI* web_ui);

  DriveInternalsUI(const DriveInternalsUI&) = delete;
  DriveInternalsUI& operator=(const DriveInternalsUI&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_DRIVE_INTERNALS_DRIVE_INTERNALS_UI_H_
