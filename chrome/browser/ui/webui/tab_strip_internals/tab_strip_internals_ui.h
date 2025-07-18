// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"

class TabStripInternalsUI;

// Registers chrome://tab-strip-internals as a debug-only WebUI that
// is conditionally enabled via the `kInternalOnlyUisEnabled` pref.
class TabStripInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<TabStripInternalsUI> {
 public:
  TabStripInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUITabStripInternalsHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The Web UI controller for the chrome://tab-strip-internals page.
class TabStripInternalsUI : public content::WebUIController {
 public:
  explicit TabStripInternalsUI(content::WebUI* web_ui);
  ~TabStripInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_INTERNALS_TAB_STRIP_INTERNALS_UI_H_
