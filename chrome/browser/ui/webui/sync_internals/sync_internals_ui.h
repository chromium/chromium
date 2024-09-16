// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_

#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class SyncInternalsUI;

class SyncInternalsUIConfig
    : public content::DefaultWebUIConfig<SyncInternalsUI> {
 public:
  SyncInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISyncInternalsHost) {}
};

// The implementation for the chrome://sync-internals page.
class SyncInternalsUI : public content::WebUIController {
 public:
  explicit SyncInternalsUI(content::WebUI* web_ui);

  SyncInternalsUI(const SyncInternalsUI&) = delete;
  SyncInternalsUI& operator=(const SyncInternalsUI&) = delete;

  ~SyncInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_
