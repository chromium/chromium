// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_UI_H_

#include "chrome/browser/ui/webui/ash/arc_graphics_tracing/arc_graphics_tracing.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
}

namespace ash {

template <ArcGraphicsTracingMode mode>
class ArcGraphicsTracingUI;

// WebUIConfig for chrome://arc-graphics-tracing and
// chrome://arc-overview-tracing
template <ArcGraphicsTracingMode mode>
class ArcGraphicsTracingUIConfig
    : public content::DefaultWebUIConfig<ArcGraphicsTracingUI<mode>> {
 public:
  ArcGraphicsTracingUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// WebUI controller for arc graphics/overview tracing.
template <ArcGraphicsTracingMode mode>
class ArcGraphicsTracingUI : public content::WebUIController {
 public:
  explicit ArcGraphicsTracingUI(content::WebUI* web_ui);

  ArcGraphicsTracingUI(const ArcGraphicsTracingUI&) = delete;
  ArcGraphicsTracingUI& operator=(const ArcGraphicsTracingUI&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_UI_H_
