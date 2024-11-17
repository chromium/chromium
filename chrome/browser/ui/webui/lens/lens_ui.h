// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LENS_LENS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LENS_LENS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class LensUI;

class LensUIConfig : public content::DefaultWebUIConfig<LensUI> {
 public:
  LensUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUILensOverlayHost) {}
};

// The WebUI for chrome://lens
class LensUI : public content::WebUIController {
 public:
  explicit LensUI(content::WebUI* web_ui);
  ~LensUI() override;
};
#endif  // CHROME_BROWSER_UI_WEBUI_LENS_LENS_UI_H_
