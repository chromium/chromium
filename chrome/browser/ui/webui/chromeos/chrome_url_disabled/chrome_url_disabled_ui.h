// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHROME_URL_DISABLED_CHROME_URL_DISABLED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHROME_URL_DISABLED_CHROME_URL_DISABLED_UI_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace chromeos {

class ChromeURLDisabledUI;

class ChromeURLDisabledUIConfig
    : public content::DefaultWebUIConfig<ChromeURLDisabledUI> {
 public:
  ChromeURLDisabledUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIAppDisabledHost) {}
};

// For chrome:://.* error page when disabled by admin policy.
class ChromeURLDisabledUI : public content::WebUIController {
 public:
  explicit ChromeURLDisabledUI(content::WebUI* web_ui);
  ~ChromeURLDisabledUI() override;

 private:
  base::WeakPtrFactory<ChromeURLDisabledUI> weak_factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CHROME_URL_DISABLED_CHROME_URL_DISABLED_UI_H_
