// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/interaction/interaction_test_util_browser.h"

#include <memory>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/user_education/common/new_badge_controller.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
constexpr char kDocumentWithTitle1URL[] = "/title1.html";
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";
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
  RunTestSequence(SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                                          kSkipPixelTestsReason),
                  // This adds a callback that calls
                  // InteractionTestUtilBrowser::CompareScreenshot().
                  Screenshot(kToolbarAppMenuButtonElementId,
                             /*screenshot_name=*/"AppMenuButton",
                             /*baseline_cl=*/"3924454"));
}

namespace {

class ScreenshotSurfaceTestDialog : public views::BubbleDialogDelegateView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTitleElementId);

  explicit ScreenshotSurfaceTestDialog(View* anchor_view)
      : views::BubbleDialogDelegateView(anchor_view,
                                        views::BubbleBorder::TOP_CENTER) {
    auto* const layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical);
    auto* const label = AddChildView(std::make_unique<views::Label>(
        u"The quick brown fox", views::style::CONTEXT_DIALOG_TITLE));
    label->SetProperty(views::kElementIdentifierKey, kTitleElementId);
    AddChildView(
        std::make_unique<views::Label>(u"...jumped over the lazy dogs."));
  }

  ~ScreenshotSurfaceTestDialog() override = default;
};

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ScreenshotSurfaceTestDialog,
                                      kTitleElementId);

}  // namespace

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest,
                       CompareScreenshot_Surface) {
  views::Widget* widget = nullptr;

  RunTestSequence(
      WithView(kTopContainerElementId,
               [&widget](views::View* anchor) {
                 widget = views::BubbleDialogDelegate::CreateBubble(
                     std::make_unique<ScreenshotSurfaceTestDialog>(anchor));
                 widget->Show();
               }),
      WaitForShow(ScreenshotSurfaceTestDialog::kTitleElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      ScreenshotSurface(ScreenshotSurfaceTestDialog::kTitleElementId,
                        /*screenshot_name=*/"TestDialog",
                        /*baseline_cl=*/"5495023"));

  if (widget) {
    widget->CloseNow();
  }
}

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest,
                       CompareScreenshot_WebPage) {
  // Set the browser view to a consistent size.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetWidget()->SetSize({400, 300});

  const GURL url = embedded_test_server()->GetURL(kDocumentWithTitle1URL);

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      NavigateWebContents(kWebContentsElementId, url),
      // This adds a callback that calls
      // InteractionTestUtilBrowser::CompareScreenshot().
      Screenshot(kWebContentsElementId, /*screenshot_name=*/std::string(),
                 /*baseline_cl=*/"3924454"));
}

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest, ConfirmOmnibox) {
  constexpr char16_t kNewUrl[] = u"chrome://version";

  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      EnterText(kOmniboxElementId, kNewUrl), Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(kWebContentsElementId, GURL(kNewUrl)));
}

IN_PROC_BROWSER_TEST_F(InteractionTestUtilBrowserTest,
                       ObserveFeatureEngagementInitialized) {
  RunTestSequence(ObserveState(kFeatureEngagementInitializedState, browser()),
                  WaitForState(kFeatureEngagementInitializedState, true));
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
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.SelectTab(browser_el, 2));
  EXPECT_EQ(2, tab_strip->GetActiveIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.SelectTab(tabstrip_el, 1));
  EXPECT_EQ(1, tab_strip->GetActiveIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.SelectTab(tabstrip_el, 0));
  EXPECT_EQ(0, tab_strip->GetActiveIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.SelectTab(browser_el, 3));
  EXPECT_EQ(3, tab_strip->GetActiveIndex());

  // Re-selecting the same tab shouldn't break anything.
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.SelectTab(tabstrip_el, 3));
  EXPECT_EQ(3, tab_strip->GetActiveIndex());
  EXPECT_EQ(ui::test::ActionResult::kSucceeded,
            test_util.SelectTab(browser_el, 3));
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
