// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

class BorealisInstallerUI;

// WebUIConfig for chrome://borealis-installer
class BorealisInstallerUIConfig
    : public content::DefaultWebUIConfig<BorealisInstallerUI> {
 public:
  BorealisInstallerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIBorealisInstallerHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://borealis-installer
class BorealisInstallerUI : public ui::MojoWebUIController {
 public:
  explicit BorealisInstallerUI(content::WebUI* web_ui);

  BorealisInstallerUI(const BorealisInstallerUI&) = delete;
  BorealisInstallerUI& operator=(const BorealisInstallerUI&) = delete;

  ~BorealisInstallerUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_BOREALIS_INSTALLER_BOREALIS_INSTALLER_UI_H_
