// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_UNTRUSTED_TOP_CHROME_WEB_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_UNTRUSTED_TOP_CHROME_WEB_UI_CONTROLLER_H_

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"

namespace content {
class WebUI;
}

// UntrustedWebUIController is intended for WebUI pages that process untrusted
// content. These WebUIController should never request WebUI bindings, but
// should instead use the WebUI Interface Broker to expose the individual
// interface needed.
class UntrustedTopChromeWebUIController : public TopChromeWebUIController {
 public:
  explicit UntrustedTopChromeWebUIController(content::WebUI* contents,
                                             bool enable_chrome_send = false);
  ~UntrustedTopChromeWebUIController() override;
  UntrustedTopChromeWebUIController(UntrustedTopChromeWebUIController&) =
      delete;
  UntrustedTopChromeWebUIController& operator=(
      const UntrustedTopChromeWebUIController&) = delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_UNTRUSTED_TOP_CHROME_WEB_UI_CONTROLLER_H_
