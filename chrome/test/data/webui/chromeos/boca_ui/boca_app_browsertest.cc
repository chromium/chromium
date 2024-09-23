// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/manta/features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"

class BocaAppBrowserProducerTest : public WebUIMochaBrowserTest {
 public:
  BocaAppBrowserProducerTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(std::string(ash::boca::kChromeBocaAppHost));

    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {ash::features::kBoca},
        /* disabled_features */ {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BocaAppBrowserProducerTest, TestMojoTranslationLayer) {
  RunTestWithoutTestLoader("chromeos/boca_ui/client_delegate_impl_test.js",
                           "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BocaAppBrowserProducerTest, TestMainPageLoaded) {
  RunTestWithoutTestLoader("chromeos/boca_ui/producer_main_page_test.js",
                           "mocha.run()");
}

class BocaAppBrowserConsumerTest : public WebUIMochaBrowserTest {
 public:
  BocaAppBrowserConsumerTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(std::string(ash::boca::kChromeBocaAppHost));

    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {ash::features::kBoca,
                                ash::features::kBocaConsumer},
        /* disabled_features */ {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BocaAppBrowserConsumerTest, TestMainPageLoaded) {
  RunTestWithoutTestLoader("chromeos/boca_ui/consumer_main_page_test.js",
                           "mocha.run()");
}
