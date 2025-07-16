// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_UI_H_

#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

// Forward declaration
class WatermarkUI;

// WebUIConfig for chrome://watermark
class WatermarkUIConfig : public content::DefaultWebUIConfig<WatermarkUI> {
 public:
  WatermarkUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://watermark
class WatermarkUI : public content::WebUIController {
 public:
  explicit WatermarkUI(content::WebUI* web_ui);
  ~WatermarkUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WATERMARK_WATERMARK_UI_H_
