// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}

// Renders the WebUI for chrome://apc-internals, the diagnostics page for
// Automated Password Change (APC) flows.
class APCInternalsUI : public content::WebUIController {
 public:
  explicit APCInternalsUI(content::WebUI* web_ui);

  APCInternalsUI(const APCInternalsUI&) = delete;
  APCInternalsUI& operator=(const APCInternalsUI&) = delete;

  ~APCInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APC_INTERNALS_APC_INTERNALS_UI_H_
