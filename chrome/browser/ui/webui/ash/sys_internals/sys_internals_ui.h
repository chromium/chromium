// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SYS_INTERNALS_SYS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SYS_INTERNALS_SYS_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class SysInternalsUI;

// WebUIConfig for chrome://sys-internals
class SysInternalsUIConfig
    : public content::DefaultWebUIConfig<SysInternalsUI> {
 public:
  SysInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISysInternalsHost) {}
};

// The UI controller for SysInternals page.
class SysInternalsUI : public content::WebUIController {
 public:
  explicit SysInternalsUI(content::WebUI* web_ui);

  SysInternalsUI(const SysInternalsUI&) = delete;
  SysInternalsUI& operator=(const SysInternalsUI&) = delete;

  ~SysInternalsUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SYS_INTERNALS_SYS_INTERNALS_UI_H_
