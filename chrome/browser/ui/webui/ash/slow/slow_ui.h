// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SLOW_SLOW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SLOW_SLOW_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class SlowUI;

// WebUIConfig for chrome://slow
class SlowUIConfig : public content::DefaultWebUIConfig<SlowUI> {
 public:
  SlowUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISlowHost) {}
};

// A custom WebUI that allows users to enable and disable performance tracing
// for feedback reports.
class SlowUI : public content::WebUIController {
 public:
  explicit SlowUI(content::WebUI* web_ui);

  SlowUI(const SlowUI&) = delete;
  SlowUI& operator=(const SlowUI&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SLOW_SLOW_UI_H_
