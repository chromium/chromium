// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMERCE_MOCK_COMMERCE_UI_TAB_HELPER_H_
#define CHROME_BROWSER_UI_COMMERCE_MOCK_COMMERCE_UI_TAB_HELPER_H_

#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

class MockCommerceUiTabHelper : public commerce::CommerceUiTabHelper {
 public:
  static void CreateForWebContents(content::WebContents* content);
  explicit MockCommerceUiTabHelper(content::WebContents* content);
  ~MockCommerceUiTabHelper() override;

  const gfx::Image& GetValidProductImage();

  const gfx::Image& GetInvalidProductImage();

  MOCK_METHOD(const gfx::Image&, GetProductImage, ());
  MOCK_METHOD(bool, ShouldShowPriceTrackingIconView, ());
  MOCK_METHOD(bool, ShouldShowPriceInsightsIconView, ());
  MOCK_METHOD(bool, IsPriceTracking, ());
  MOCK_METHOD(void,
              SetPriceTrackingState,
              (bool enable,
               bool is_new_bookmark,
               base::OnceCallback<void(bool)> callback),
              (override));
  MOCK_METHOD(std::unique_ptr<views::View>,
              CreateShoppingInsightsWebView,
              (),
              (override));
  MOCK_METHOD(const absl::optional<commerce::PriceInsightsInfo>&,
              GetPriceInsightsInfo,
              ());
  MOCK_METHOD(bool, ShouldExpandPageActionIcon, (PageActionIconType type));
  MOCK_METHOD(PriceInsightsIconView::PriceInsightsIconLabelType,
              GetPriceInsightsIconLabelTypeForPage,
              ());

 private:
  gfx::Image valid_product_image_;
  gfx::Image empty_product_image_;
};

#endif  // CHROME_BROWSER_UI_COMMERCE_MOCK_COMMERCE_UI_TAB_HELPER_H_
