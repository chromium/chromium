// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_test.h"

class FeatureShowcaseBrowserTest : public WebUIMochaBrowserTest {
 protected:
  FeatureShowcaseBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {switches::kFirstRunDesktopRefresh,
         switches::kFirstRunDesktopChoiceScreenRefresh,
         switches::kFirstRunDesktopRevamp},
        {});
    set_test_loader_host(chrome::kChromeUIFeatureShowcaseHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FeatureShowcaseBrowserTest, App) {
  RunTest("feature_showcase/app_test.js", "mocha.run()");
}
