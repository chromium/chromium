// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class MetricsInternalsUI;

// WebUIConfig for chrome://metrics-internals
class MetricsInternalsUIConfig
    : public content::DefaultWebUIConfig<MetricsInternalsUI> {
 public:
  MetricsInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIMetricsInternalsHost) {}
};

// Controller for the chrome://metrics-internals page.
class MetricsInternalsUI : public content::WebUIController {
 public:
  explicit MetricsInternalsUI(content::WebUI* web_ui);

  MetricsInternalsUI(const MetricsInternalsUI&) = delete;
  MetricsInternalsUI& operator=(const MetricsInternalsUI&) = delete;

  ~MetricsInternalsUI() override = default;
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_UI_H_
