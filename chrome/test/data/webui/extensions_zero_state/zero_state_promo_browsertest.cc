// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "content/public/test/browser_test.h"

class ZeroStatePromoBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ZeroStatePromoBrowserTest() {
    set_test_loader_host(chrome::kChromeUIExtensionsZeroStatePromoHost);
  }
};

class ZeroStatePromoChipsUiTest : public ZeroStatePromoBrowserTest {
 protected:
  ZeroStatePromoChipsUiTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHExtensionsZeroStatePromoFeature,
        {{feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.name,
          feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.GetName(
              feature_engagement::IPHExtensionsZeroStatePromoVariant::
                  kCustomUiChipIph)}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZeroStatePromoChipsUiTest, AllTests) {
  ZeroStatePromoBrowserTest::RunTest(
      "extensions_zero_state/zero_state_promo_app_test.js",
      "runMochaSuite('ChipsUiTest');");
}

class ZeroStatePromoPlainLinkUiTest : public ZeroStatePromoBrowserTest {
 protected:
  ZeroStatePromoPlainLinkUiTest() : ZeroStatePromoBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        feature_engagement::kIPHExtensionsZeroStatePromoFeature,
        {{feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.name,
          feature_engagement::kIPHExtensionsZeroStatePromoVariantParam.GetName(
              feature_engagement::IPHExtensionsZeroStatePromoVariant::
                  kCustomUIPlainLinkIph)}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ZeroStatePromoPlainLinkUiTest, AllTests) {
  ZeroStatePromoBrowserTest::RunTest(
      "extensions_zero_state/zero_state_promo_app_test.js",
      "runMochaSuite('PlainLinkUiTest');");
}
