// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ui_devtools/devtools_server.h"
#include "components/ui_devtools/switches.h"
#include "components/ui_devtools/views/server_holder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class UIDevToolsBrowserTest : public InProcessBrowserTest {
 public:
  UIDevToolsBrowserTest() = default;
  ~UIDevToolsBrowserTest() override = default;

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    // Launch UI DevTools with port 0 (--enable-ui-devtools=0). This ensures the
    // server listens on an available ephemeral port, avoiding "Address already
    // in use" errors which were causing the test to fail/timeout.
    command_line->AppendSwitchASCII(ui_devtools::switches::kEnableUiDevTools,
                                    "0");
  }
};

// Regression test for crbug.com/487264576. UI DevTools could not be connected
// to due to a CSP rule in the DevTools frontend.
IN_PROC_BROWSER_TEST_F(UIDevToolsBrowserTest, Connection) {
  // Wait for the server to start.
  const ui_devtools::UiDevToolsServer* server =
      ui_devtools::ServerHolder::GetInstance()->GetUiDevToolsServerInstance();
  ASSERT_TRUE(server);

  // Construct the URL for the frontend.
  // Use 127.0.0.1 as per the server implementation and the reported error.
  std::string url_string = base::StringPrintf(
      "devtools://devtools/bundled/"
      "devtools_app.html?uiDevTools=true&ws=127.0.0.1:%d/0",
      server->port());
  GURL url(url_string);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern("*Content Security Policy*");

  base::RunLoop run_loop;
  const_cast<ui_devtools::UiDevToolsServer*>(server)
      ->SetOnClientConnectedForTesting(run_loop.QuitClosure());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  run_loop.Run();

  // If the connection violates CSP, an error will be logged to the console.
  if (!console_observer.messages().empty()) {
    FAIL() << "CSP violation detected: " << console_observer.GetMessageAt(0);
  }
}

}  // namespace
