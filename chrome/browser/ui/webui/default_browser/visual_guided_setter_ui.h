// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_UI_H_

#include <string_view>

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

class VisualGuidedSetterUI;

namespace content {
class WebUI;
}
class VisualGuidedSetterUIConfig final
    : public content::DefaultWebUIConfig<VisualGuidedSetterUI> {
 public:
  // NOLINTNEXTLINE(modernize-use-equals-default)
  VisualGuidedSetterUIConfig()
      : DefaultWebUIConfig(
            content::kChromeUIScheme,
            chrome::kChromeUIDefaultBrowserVisualGuidedSetterHost) {}
};

class VisualGuidedSetterUI final : public ui::MojoWebUIController {
 public:
  explicit VisualGuidedSetterUI(content::WebUI* web_ui);

  VisualGuidedSetterUI(const VisualGuidedSetterUI&) = delete;
  VisualGuidedSetterUI& operator=(const VisualGuidedSetterUI&) = delete;

  ~VisualGuidedSetterUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_UI_H_
