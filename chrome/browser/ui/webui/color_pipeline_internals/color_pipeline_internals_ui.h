// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COLOR_PIPELINE_INTERNALS_COLOR_PIPELINE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COLOR_PIPELINE_INTERNALS_COLOR_PIPELINE_INTERNALS_UI_H_

#include "content/public/browser/internal_webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

class ColorPipelineInternalsUI;

class ColorPipelineInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<ColorPipelineInternalsUI> {
 public:
  ColorPipelineInternalsUIConfig();
};

// Client could put debug WebUI as sub-URL under chrome://internals/.
// e.g. chrome://internals/your-feature.
class ColorPipelineInternalsUI : public ui::MojoWebUIController {
 public:
  explicit ColorPipelineInternalsUI(content::WebUI* web_ui);
  ~ColorPipelineInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

};

#endif  // CHROME_BROWSER_UI_WEBUI_COLOR_PIPELINE_INTERNALS_COLOR_PIPELINE_INTERNALS_UI_H_
