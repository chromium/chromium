// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using SidePanelReadingListTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(SidePanelReadingListTest, App) {
  set_test_loader_host(chrome::kChromeUIReadLaterHost);
  RunTest("side_panel/reading_list/reading_list_app_test.js", "mocha.run()");
}
