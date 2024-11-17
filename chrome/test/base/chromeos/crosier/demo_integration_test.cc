// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/gtest_tags.h"
#include "base/test/test_switches.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"
#include "chrome/test/base/chromeos/crosier/crosier_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

// This is a demo to run a browser test on a ChromeOS device or VM.
//
// To run the test:
//
// 1. See http://go/crosint-run for instructions.
// 2. You can optionally add --test-launcher-interactive to play with the
//    browser after the test finishes.
class DemoIntegrationTest : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // You don't need this in your test.
    // By default, we don't allow any network access in tests to
    // avoid flakiness. This is just a showcase so people can
    // interact with the browser try out a few websites.
    host_resolver()->AllowDirectLookup("*.google.com");
    host_resolver()->AllowDirectLookup("*.geotrust.com");
    host_resolver()->AllowDirectLookup("*.gstatic.com");
    host_resolver()->AllowDirectLookup("*.googleapis.com");
    host_resolver()->AllowDirectLookup("accounts.google.*");
    host_resolver()->AllowDirectLookup("*.chromium.org");

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    // Close any browsers we opened otherwise the test may hang on shutdown.
    // This happens because chromeos_integration_tests is not started by session
    // manager, but AttemptUserExit() uses session manager to kill the chrome
    // binary.
    // TODO(b/292067979): Find a better way to work around this issue.
    auto* browser_list = BrowserList::GetInstance();
    // Copy the browser list to avoid mutating it during iteration.
    std::vector<Browser*> browsers(browser_list->begin(), browser_list->end());
    for (Browser* browser : browsers) {
      CloseBrowserSynchronously(browser);
    }

    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DemoIntegrationTest, NewTab) {
  chrome_test_base_chromeos_crosier::TestInfo info;
  info.set_description(R"(
This test verifies Chrome can launch and open version page.
Manually:
  1 Launch Chrome
  2 Go to chrome://version
  3 Make sure the page can open successfully.)");
  info.add_contacts("svenzheng@chromium.org");
  info.add_contacts("jamescook@chromium.org");
  info.set_team_email("crosier-team@google.com");
  info.set_buganizer("1394295");
  crosier_util::AddTestInfo(info);
  base::AddFeatureIdTagToTestResult(
      "screenplay-351d628b-e4a4-41c6-91e4-a4036ad12360");

  GURL version_url{"chrome://version"};

  ASSERT_TRUE(AddTabAtIndex(0, version_url, ui::PAGE_TRANSITION_TYPED));

  // You don't need these for your tests. This is just to showcase the browser
  // when you want to see the test browser and interact with it.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherInteractive)) {
    base::RunLoop loop;
    loop.Run();
  }
}
