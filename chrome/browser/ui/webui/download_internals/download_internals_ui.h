// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"

class DownloadInternalsUI;

class DownloadInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<DownloadInternalsUI> {
 public:
  DownloadInternalsUIConfig()
      : DefaultInternalWebUIConfig(chrome::kChromeUIDownloadInternalsHost) {}
};

// The WebUI for chrome://download-internals.
class DownloadInternalsUI : public content::WebUIController {
 public:
  explicit DownloadInternalsUI(content::WebUI* web_ui);

  DownloadInternalsUI(const DownloadInternalsUI&) = delete;
  DownloadInternalsUI& operator=(const DownloadInternalsUI&) = delete;

  ~DownloadInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_UI_H_
