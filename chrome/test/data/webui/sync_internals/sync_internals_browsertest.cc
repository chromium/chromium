// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"

class SyncInternalsWebUITest : public WebUIMochaBrowserTest {
 protected:
  SyncInternalsWebUITest() {
    set_test_loader_host(chrome::kChromeUISyncInternalsHost);
  }

  void RunTestCase(const std::string& testCase) {
    RunTestWithoutTestLoader(
        "sync_internals/sync_internals_test.js",
        base::StringPrintf("runMochaTest('SyncInternals', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, Uninitialized) {
  RunTestCase("Uninitialized");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, SyncDisabledByDefaultChromeOS) {
  RunTestCase("SyncDisabledByDefaultChromeOS");
}
#else
IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, SyncDisabledByDefault) {
  RunTestCase("SyncDisabledByDefault");
}
#endif

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, LoadPastedAboutInfo) {
  RunTestCase("LoadPastedAboutInfo");
}

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, NetworkEventsTest) {
  RunTestCase("NetworkEventsTest");
}

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest,
                       SearchTabDoesntChangeOnItemSelect) {
  RunTestCase("SearchTabDoesntChangeOnItemSelect");
}

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, NodeBrowserTest) {
  RunTestCase("NodeBrowserTest");
}

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, NodeBrowserRefreshOnTabSelect) {
  RunTestCase("NodeBrowserRefreshOnTabSelect");
}

IN_PROC_BROWSER_TEST_F(SyncInternalsWebUITest, DumpSyncEventsToText) {
  RunTestCase("DumpSyncEventsToText");
}
