// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_UI_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_UI_HANDLER_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/commerce/core/webui/shopping_list_handler.h"

namespace commerce {

class ShoppingUiHandlerDelegate : public ShoppingListHandler::Delegate {
 public:
  ShoppingUiHandlerDelegate();
  ShoppingUiHandlerDelegate(const ShoppingUiHandlerDelegate&) = delete;
  ShoppingUiHandlerDelegate& operator=(const ShoppingUiHandlerDelegate&) =
      delete;
  ~ShoppingUiHandlerDelegate() override;

  absl::optional<GURL> GetCurrentTabUrl() override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_UI_WEBUI_COMMERCE_SHOPPING_UI_HANDLER_DELEGATE_H_
