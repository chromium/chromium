// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class WhatsNewBrowserTest : public WebUIMochaBrowserTest {
 protected:
  WhatsNewBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(whats_new::kForceEnabled);
    set_test_loader_host(chrome::kChromeUIWhatsNewHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

typedef WhatsNewBrowserTest WhatsNewAppTest;
IN_PROC_BROWSER_TEST_F(WhatsNewAppTest, All) {
  RunTest("whats_new/whats_new_app_test.js", "mocha.run();");
}
