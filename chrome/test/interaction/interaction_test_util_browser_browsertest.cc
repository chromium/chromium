// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
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
  // Set the browser view to a consistent size.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetWidget()->SetSize({400, 300});

  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);

  RunTestSequence(InstrumentTab(kWebContentsElementId),
                  NavigateWebContents(kWebContentsElementId, url),
                  // This adds a callback that calls
                  // InteractionTestUtilBrowser::CompareScreenshot().
                  Screenshot(kWebContentsElementId, std::string(), "3924454"));
}

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest, ConfirmOmnibox) {
  constexpr char16_t kNewUrl[] = u"chrome://version";

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      EnterText(kOmniboxElementId, kNewUrl), Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(kWebContentsElementId, GURL(kNewUrl)));
}

class InteractionTestUtilBrowserSelectTabTest
    : public InteractionTestUtilBrowserTest,
      public testing::WithParamInterface<
          ui::test::InteractionTestUtil::InputType> {
 public:
  InteractionTestUtilBrowserSelectTabTest() = default;
  ~InteractionTestUtilBrowserSelectTabTest() override = default;
};

IN_PROC_BROWSER_TEST_P(InteractionTestUtilBrowserSelectTabTest, SelectTab) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  TabStrip* const tab_strip = browser_view->tabstrip();
  auto* const browser_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(browser_view,
                                                                   true);
  auto* const tabstrip_el =
      views::ElementTrackerViews::GetInstance()->GetElementForView(tab_strip,
                                                                   true);

  // Add up to a total of four tabs.
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  CHECK(AddTabAtIndex(-1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));

  // Select a few different tabs using both the browser and tabstrip as targets.
  InteractionTestUtilBrowser test_util;
  test_util.SelectTab(browser_el, 2);
  EXPECT_EQ(2, tab_strip->GetActiveIndex());
  test_util.SelectTab(tabstrip_el, 1);
  EXPECT_EQ(1, tab_strip->GetActiveIndex());
  test_util.SelectTab(tabstrip_el, 0);
  EXPECT_EQ(0, tab_strip->GetActiveIndex());
  test_util.SelectTab(browser_el, 3);
  EXPECT_EQ(3, tab_strip->GetActiveIndex());

  // Re-selecting the same tab shouldn't break anything.
  test_util.SelectTab(tabstrip_el, 3);
  EXPECT_EQ(3, tab_strip->GetActiveIndex());
  test_util.SelectTab(browser_el, 3);
  EXPECT_EQ(3, tab_strip->GetActiveIndex());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    InteractionTestUtilBrowserSelectTabTest,
    ::testing::Values(ui::test::InteractionTestUtil::InputType::kDontCare,
                      ui::test::InteractionTestUtil::InputType::kMouse,
                      ui::test::InteractionTestUtil::InputType::kKeyboard,
                      ui::test::InteractionTestUtil::InputType::kTouch),
    [](testing::TestParamInfo<ui::test::InteractionTestUtil::InputType>
           input_type) -> std::string {
      switch (input_type.param) {
        case ui::test::InteractionTestUtil::InputType::kDontCare:
          return "DontCare";
        case ui::test::InteractionTestUtil::InputType::kMouse:
          return "Mouse";
        case ui::test::InteractionTestUtil::InputType::kKeyboard:
          return "Keyboard";
        case ui::test::InteractionTestUtil::InputType::kTouch:
          return "Touch";
      }
    });
