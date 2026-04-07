// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class FeatureShowcaseUI;

// The WebUIConfig for `chrome://feature-showcase`.
class FeatureShowcaseUIConfig
    : public content::DefaultWebUIConfig<FeatureShowcaseUI> {
 public:
  FeatureShowcaseUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUIController for `chrome://feature-showcase`.
class FeatureShowcaseUI : public content::WebUIController {
 public:
  explicit FeatureShowcaseUI(content::WebUI* web_ui);
  FeatureShowcaseUI(const FeatureShowcaseUI&) = delete;
  FeatureShowcaseUI& operator=(const FeatureShowcaseUI&) = delete;
  ~FeatureShowcaseUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_FEATURE_SHOWCASE_UI_H_
