// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CRYPTOHOME_CRYPTOHOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CRYPTOHOME_CRYPTOHOME_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

class CryptohomeUI;

// WebUIConfig for chrome://cryptohome
class CryptohomeUIConfig : public content::DefaultWebUIConfig<CryptohomeUI> {
 public:
  CryptohomeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICryptohomeHost) {}
};

// WebUIController for chrome://cryptohome.
class CryptohomeUI : public content::WebUIController {
 public:
  explicit CryptohomeUI(content::WebUI* web_ui);

  CryptohomeUI(const CryptohomeUI&) = delete;
  CryptohomeUI& operator=(const CryptohomeUI&) = delete;

  ~CryptohomeUI() override {}
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CRYPTOHOME_CRYPTOHOME_UI_H_
