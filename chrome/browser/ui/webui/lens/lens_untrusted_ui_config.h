// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LENS_LENS_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_LENS_LENS_UNTRUSTED_UI_CONFIG_H_

#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace lens {

// The configuration for the chrome-untrusted://lens page.
class LensUntrustedUIConfig : public content::WebUIConfig {
 public:
  LensUntrustedUIConfig();
  ~LensUntrustedUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_WEBUI_LENS_LENS_UNTRUSTED_UI_CONFIG_H_
