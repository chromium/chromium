// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_COMMERCE_INTERNALS_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_COMMERCE_INTERNALS_UI_CONFIG_H_

#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace commerce {

class CommerceInternalsUIConfig : public content::WebUIConfig {
 public:
  CommerceInternalsUIConfig();
  ~CommerceInternalsUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_COMMERCE_INTERNALS_UI_CONFIG_H_
