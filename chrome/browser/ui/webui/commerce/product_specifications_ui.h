// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "url/gurl.h"

namespace commerce {

class ProductSpecificationsUI : public content::WebUIController {
 public:
  explicit ProductSpecificationsUI(content::WebUI* web_ui);
  ~ProductSpecificationsUI() override;
};

class ProductSpecificationsUIConfig : public content::WebUIConfig {
 public:
  ProductSpecificationsUIConfig();
  ~ProductSpecificationsUIConfig() override;

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_H_
