// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_test.h"

class WhatsNewBrowserTest : public WebUIMochaBrowserTest {
 protected:
  WhatsNewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {whats_new::kForceEnabled},
        {user_education::features::kWhatsNewVersion2});
    set_test_loader_host(chrome::kChromeUIWhatsNewHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

typedef WhatsNewBrowserTest WhatsNewAppTest;
IN_PROC_BROWSER_TEST_F(WhatsNewAppTest, All) {
  RunTest("whats_new/whats_new_app_test.js", "mocha.run();");
}

class WhatsNewV2BrowserTest : public WebUIMochaBrowserTest {
 protected:
  WhatsNewV2BrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {user_education::features::kWhatsNewVersion2, whats_new::kForceEnabled},
        {});
    set_test_loader_host(chrome::kChromeUIWhatsNewHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

typedef WhatsNewV2BrowserTest WhatsNewV2AppTest;
IN_PROC_BROWSER_TEST_F(WhatsNewV2AppTest, All) {
  RunTest("whats_new/whats_new_v2_app_test.js", "mocha.run();");
}
