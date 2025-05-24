// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash::cellular_setup {

class MobileSetupUI;

class MobileSetupUIConfig : public content::DefaultWebUIConfig<MobileSetupUI> {
 public:
  MobileSetupUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIMobileSetupHost) {}
};

// DEPRECATED: Being replaced by new UI; see https://crbug.com/778021.
class MobileSetupUI : public ui::WebDialogUI {
 public:
  explicit MobileSetupUI(content::WebUI* web_ui);

  MobileSetupUI(const MobileSetupUI&) = delete;
  MobileSetupUI& operator=(const MobileSetupUI&) = delete;

  ~MobileSetupUI() override;
};

}  // namespace ash::cellular_setup

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CELLULAR_SETUP_MOBILE_SETUP_UI_H_
