// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class TabSearchFocusTest : public WebUIMochaBrowserTest {
 protected:
  TabSearchFocusTest() { set_test_loader_host(chrome::kChromeUITabSearchHost); }
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_App DISABLED_App
#else
#define MAYBE_App App
#endif
IN_PROC_BROWSER_TEST_F(TabSearchFocusTest, MAYBE_App) {
  RunTest("tab_search/tab_search_page_focus_test.js", "mocha.run()");
}

// Some of the organization tests require checking focus and blur logic. This
// must be run as an interactive_ui_test.
IN_PROC_BROWSER_TEST_F(TabSearchFocusTest, Organization) {
  RunTest("tab_search/auto_tab_groups_page_test.js", "mocha.run()");
}
