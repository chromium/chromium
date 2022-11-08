// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class MultideviceInternalsUI;

// WebUIConfig for chrome://multidevice-internals
class MultideviceInternalsUIConfig
    : public content::DefaultWebUIConfig<MultideviceInternalsUI> {
 public:
  MultideviceInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIMultiDeviceInternalsHost) {}
};

// The WebUI controller for chrome://multidevice-internals.
class MultideviceInternalsUI : public ui::MojoWebUIController {
 public:
  explicit MultideviceInternalsUI(content::WebUI* web_ui);
  MultideviceInternalsUI(const MultideviceInternalsUI&) = delete;
  MultideviceInternalsUI& operator=(const MultideviceInternalsUI&) = delete;
  ~MultideviceInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MULTIDEVICE_INTERNALS_MULTIDEVICE_INTERNALS_UI_H_
