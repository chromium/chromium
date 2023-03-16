// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_MOCK_SHOPPING_LIST_UI_TAB_HELPER_H_
#define CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_MOCK_SHOPPING_LIST_UI_TAB_HELPER_H_

#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}  // namespace content

class MockShoppingListUiTabHelper : public commerce::ShoppingListUiTabHelper {
 public:
  static void CreateForWebContents(content::WebContents* content);
  explicit MockShoppingListUiTabHelper(content::WebContents* content);
  ~MockShoppingListUiTabHelper() override;

  const gfx::Image& GetValidProductImage();

  const gfx::Image& GetInvalidProductImage();

  MOCK_METHOD0(GetProductImage, const gfx::Image&());
  MOCK_METHOD0(ShouldShowPriceTrackingIconView, bool());
  MOCK_METHOD0(IsPriceTracking, bool());
  MOCK_METHOD(void,
              SetPriceTrackingState,
              (bool enable,
               bool is_new_bookmark,
               base::OnceCallback<void(bool)> callback),
              (override));

 private:
  gfx::Image valid_product_image_;
  gfx::Image empty_product_image_;
};

#endif  // CHROME_BROWSER_UI_COMMERCE_PRICE_TRACKING_MOCK_SHOPPING_LIST_UI_TAB_HELPER_H_
