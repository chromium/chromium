// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_OVERVIEW_TRACING_ARC_OVERVIEW_TRACING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_OVERVIEW_TRACING_ARC_OVERVIEW_TRACING_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
}

namespace ash {

class ArcOverviewTracingUI;

// WebUIConfig for chrome://arc-overview-tracing
class ArcOverviewTracingUIConfig
    : public content::DefaultWebUIConfig<ArcOverviewTracingUI> {
 public:
  ArcOverviewTracingUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for arc graphics/overview tracing.
class ArcOverviewTracingUI : public content::WebUIController {
 public:
  explicit ArcOverviewTracingUI(content::WebUI* web_ui);

  ArcOverviewTracingUI(const ArcOverviewTracingUI&) = delete;
  ArcOverviewTracingUI& operator=(const ArcOverviewTracingUI&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_OVERVIEW_TRACING_ARC_OVERVIEW_TRACING_UI_H_
