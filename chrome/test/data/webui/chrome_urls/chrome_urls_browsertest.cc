// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using ChromeURLsUiBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeURLsUiBrowserTest, App) {
  set_test_loader_host(chrome::kChromeUIChromeURLsHost);
  RunTest("chrome_urls/app_test.js", "mocha.run()");
}
