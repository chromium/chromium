// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"

class IwaDevBrowserTest : public WebUIMochaBrowserTest {
 protected:
  IwaDevBrowserTest() {
    set_test_loader_host(chrome::kChromeUIIwaDevHost);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebAppDevUi,
                              features::kIsolatedWebApps},
        /*disabled_features=*/{});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IwaDevBrowserTest, App) {
  RunTest("iwa_dev/app_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(IwaDevBrowserTest, InstallDialog) {
  RunTest("iwa_dev/install_dialog_test.js", "mocha.run();");
}
