// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest DownloadsFocusTest;

// TODO(crbug.com/489979009): Re-enable when the test is not flaky anymore.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Item DISABLED_Item
#else
#define MAYBE_Item Item
#endif
IN_PROC_BROWSER_TEST_F(DownloadsFocusTest, MAYBE_Item) {
  set_test_loader_host(chrome::kChromeUIDownloadsHost);
  RunTest("downloads/item_test.js", "runMochaSuite('ItemFocusTest')");
}
