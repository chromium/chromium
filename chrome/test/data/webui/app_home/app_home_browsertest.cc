// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class AppHomeTest : public WebUIMochaBrowserTest {
 protected:
  AppHomeTest() { set_test_loader_host(chrome::kChromeUIAppLauncherPageHost); }
};

IN_PROC_BROWSER_TEST_F(AppHomeTest, AppList) {
  RunTest("app_home/app_list_test.js", "mocha.run()");
}
