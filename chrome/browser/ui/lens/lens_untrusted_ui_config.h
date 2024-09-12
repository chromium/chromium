// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_UI_LENS_LENS_UNTRUSTED_UI_CONFIG_H_

#include "chrome/browser/ui/lens/lens_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "content/public/common/url_constants.h"

namespace lens {

// The configuration for the chrome-untrusted://lens page.
class LensUntrustedUIConfig
    : public DefaultTopChromeWebUIConfig<LensUntrustedUI> {
 public:
  LensUntrustedUIConfig();
  ~LensUntrustedUIConfig() override = default;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_UNTRUSTED_UI_CONFIG_H_
