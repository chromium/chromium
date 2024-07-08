// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "content/public/test/browser_test.h"

class RelatedWebsiteSetsTest : public WebUIMochaBrowserTest {
 protected:
  RelatedWebsiteSetsTest() {
    scoped_feature_list_.InitWithFeatures(
      {privacy_sandbox::kRelatedWebsiteSetsDevUI,
       privacy_sandbox::kPrivacySandboxInternalsDevUI},
      {});
    set_test_loader_host(chrome::kChromeUIPrivacySandboxInternalsHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RelatedWebsiteSetsTest, App) {
  RunTest("privacy_sandbox/internals/related_website_sets/app_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(RelatedWebsiteSetsTest, Container) {
  RunTest("privacy_sandbox/internals/related_website_sets/container_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(RelatedWebsiteSetsTest, ListItem) {
  RunTest("privacy_sandbox/internals/related_website_sets/list_item_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(RelatedWebsiteSetsTest, Toolbar) {
  RunTest("privacy_sandbox/internals/related_website_sets/toolbar_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(RelatedWebsiteSetsTest, Sidebar) {
  RunTest("privacy_sandbox/internals/related_website_sets/sidebar_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(RelatedWebsiteSetsTest, SiteFavicon) {
  RunTest("privacy_sandbox/internals/related_website_sets/site_favicon_test.js",
          "mocha.run()");
}
