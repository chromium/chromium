// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
constexpr char kDocumentWithTitle1URL[] = "/title1.html";
}

class InteractionTestUtilBrowserTest : public InteractiveBrowserTest {
 public:
  InteractionTestUtilBrowserTest() = default;
  ~InteractionTestUtilBrowserTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest, GetBrowserFromContext) {
  Browser* const other_browser = CreateBrowser(browser()->profile());
  EXPECT_EQ(browser(), InteractionTestUtilBrowser::GetBrowserFromContext(
                           browser()->window()->GetElementContext()));
  EXPECT_EQ(other_browser, InteractionTestUtilBrowser::GetBrowserFromContext(
                               other_browser->window()->GetElementContext()));
}

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest, CompareScreenshot_View) {
  RunTestSequence(
      // This adds a callback that calls
      // InteractionTestUtilBrowser::CompareScreenshot().
      Screenshot(kAppMenuButtonElementId, "AppMenuButton", "3924454"));
}

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest,
                       CompareScreenshot_WebPage) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);

  // Set the browser view to a consistent size.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetWidget()->SetSize({400, 300});

  InstrumentTab(browser(), kWebContentsElementId);
  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);

  RunTestSequence(NavigateWebContents(kWebContentsElementId, url),
                  // This adds a callback that calls
                  // InteractionTestUtilBrowser::CompareScreenshot().
                  Screenshot(kWebContentsElementId, std::string(), "3924454"));
}
