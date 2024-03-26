// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"
class ProductSpecificationsTest : public WebUIMochaBrowserTest {
 protected:
  ProductSpecificationsTest() {
    set_test_loader_host(commerce::kChromeUICompareHost);
    scoped_feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsTest, App) {
  RunTest("commerce/product_specifications_app_test.js", "mocha.run()");
}
