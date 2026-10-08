// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SplitViewTest : public WebUIMochaBrowserTest {
 protected:
  SplitViewTest() { set_test_loader_host(chrome::kChromeUITabSearchHost); }
};

// TODO(crbug.com/571185748): Flaky (hangs after mocha completes) on Mac and
// Win-arm64.
#if BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64))
#define MAYBE_SplitNewTabPage DISABLED_SplitNewTabPage
#else
#define MAYBE_SplitNewTabPage SplitNewTabPage
#endif
IN_PROC_BROWSER_TEST_F(SplitViewTest, MAYBE_SplitNewTabPage) {
  RunTest("tab_search/split_new_tab_page_test.js", "mocha.run()");
}
