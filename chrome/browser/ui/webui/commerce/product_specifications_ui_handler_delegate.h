// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_HANDLER_DELEGATE_H_

#include "components/commerce/core/webui/product_specifications_handler.h"
#include "content/public/browser/web_ui.h"

namespace commerce {

class ProductSpecificationsUIHandlerDelegate
    : public ProductSpecificationsHandler::Delegate {
 public:
  explicit ProductSpecificationsUIHandlerDelegate(content::WebUI* web_ui);
  ProductSpecificationsUIHandlerDelegate(
      const ProductSpecificationsUIHandlerDelegate&) = delete;
  ProductSpecificationsUIHandlerDelegate& operator=(
      const ProductSpecificationsUIHandlerDelegate&) = delete;
  ~ProductSpecificationsUIHandlerDelegate() override;

  void ShowSyncSetupFlow() override;

  void ShowDisclosureDialog(const std::vector<GURL>& urls,
                            const std::string& name,
                            const std::string& set_id) override;

  void ShowProductSpecificationsSetForUuid(const base::Uuid& uuid,
                                           bool in_new_tab) override;

 private:
  raw_ptr<content::WebUI> web_ui_;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_PRODUCT_SPECIFICATIONS_UI_HANDLER_DELEGATE_H_
