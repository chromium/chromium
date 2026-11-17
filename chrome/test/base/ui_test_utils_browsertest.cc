// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include <algorithm>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"

class UITestUtilsBrowserTest : public InProcessBrowserTest {
 public:
  UITestUtilsBrowserTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This is a regression test for crbug.com/40172821, where
// NavigateToURLWithDisposition incorrectly inferred the index in the tab strip
// of the second background tab and waited for the wrong tab to finish loading
// (which never happened).
IN_PROC_BROWSER_TEST_F(UITestUtilsBrowserTest, OpenTwoTabsInBackground) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("a.com", "/title2.html");
  const GURL url3 = embedded_test_server()->GetURL("a.com", "/title3.html");

  NavigateToURLWithDisposition(browser(), url1,
                               ui::mojom::WindowOpenDisposition::kCurrentTab,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  NavigateToURLWithDisposition(
      browser(), url2, ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // This test will pass if this call does not time out, which requires
  //   // NavigateToURLWithDisposition to pick correct tab to wait for.
  NavigateToURLWithDisposition(
      browser(), url3, ui::mojom::WindowOpenDisposition::kNewBackgroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

IN_PROC_BROWSER_TEST_F(UITestUtilsBrowserTest, FindMatchingBrowsers) {
  BrowserWindowInterface* const regular_browser1 = browser();
  BrowserWindowInterface* const regular_browser2 =
      CreateBrowser(browser()->GetProfile());
  BrowserWindowInterface* const incognito_browser = CreateIncognitoBrowser();

  // All browsers.
  auto all_browsers = ui_test_utils::FindMatchingBrowsers(
      [](BrowserWindowInterface*) { return true; });
  EXPECT_EQ(3u, all_browsers.size());
  EXPECT_TRUE(std::ranges::contains(all_browsers, regular_browser1));
  EXPECT_TRUE(std::ranges::contains(all_browsers, regular_browser2));
  EXPECT_TRUE(std::ranges::contains(all_browsers, incognito_browser));

  // No browsers.
  auto no_browsers = ui_test_utils::FindMatchingBrowsers(
      [](BrowserWindowInterface*) { return false; });
  EXPECT_TRUE(no_browsers.empty());

  // Incognito browsers.
  auto incognito_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return browser->GetProfile()->IsIncognitoProfile();
      });
  EXPECT_EQ(1u, incognito_browsers.size());
  EXPECT_EQ(incognito_browser, incognito_browsers[0]);

  // Regular browsers.
  auto regular_browsers =
      ui_test_utils::FindMatchingBrowsers([](BrowserWindowInterface* browser) {
        return !browser->GetProfile()->IsIncognitoProfile();
      });
  EXPECT_EQ(2u, regular_browsers.size());
  EXPECT_TRUE(std::ranges::contains(regular_browsers, regular_browser1));
  EXPECT_TRUE(std::ranges::contains(regular_browsers, regular_browser2));
}

// Verifies that BrowserCreatedObserver:
// - keeps the observed browser when an unrelated pre-existing browser closes
//   before Wait(),
// - returns the most recently created browser when multiple browsers open
//   before Wait(),
// - supports multiple sequential Wait() calls (both synchronous and
//   asynchronous), and
// - avoids dangling pointers when observed browsers close before the observer
//   leaves scope.
IN_PROC_BROWSER_TEST_F(UITestUtilsBrowserTest, BrowserCreatedObserver) {
  Profile* const profile = GetProfile();
  BrowserWindowInterface* const initial_browser = browser();

  // Closing an unrelated pre-existing browser before Wait() must not clear the
  // observed browser.
  {
    ui_test_utils::BrowserCreatedObserver observer;
    BrowserWindowInterface* const first_created = CreateBrowser(profile);
    CloseBrowserSynchronously(initial_browser);
    EXPECT_EQ(first_created, observer.Wait());
  }

  // Opening multiple browsers before Wait() returns the most recently created
  // browser, and reusing the same observer for subsequent synchronous or
  // asynchronous creations returns each newly created browser without dangling
  // pointer failures when browsers close before `observer` is destroyed.
  {
    ui_test_utils::BrowserCreatedObserver observer;
    BrowserWindowInterface* const second_created = CreateBrowser(profile);
    BrowserWindowInterface* const third_created = CreateBrowser(profile);
    EXPECT_EQ(third_created, observer.Wait());

    BrowserWindowInterface* const fourth_created = CreateBrowser(profile);
    EXPECT_EQ(fourth_created, observer.Wait());

    BrowserWindowInterface* fifth_created = nullptr;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(
                       [&]() { fifth_created = CreateBrowser(profile); }));
    BrowserWindowInterface* const waited_fifth = observer.Wait();
    EXPECT_NE(nullptr, fifth_created);
    EXPECT_EQ(fifth_created, waited_fifth);

    // Also create and close an extra unwaited browser while `observer` is still
    // alive to ensure `observer` holds no dangling reference on destruction.
    BrowserWindowInterface* const sixth_created = CreateBrowser(profile);
    CloseBrowserSynchronously(sixth_created);
    CloseBrowserSynchronously(fifth_created);
    CloseBrowserSynchronously(fourth_created);
    CloseBrowserSynchronously(third_created);
    EXPECT_NE(nullptr, second_created);
  }
}
