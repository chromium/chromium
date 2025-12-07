// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabSearchPageElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
constexpr char kDocumentWithButtonURL[] = "/button.html";
constexpr char kDocumentWithIframe[] = "/iframe_elements.html";
}  // namespace

class WebContentsInteractionTestUtilInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  WebContentsInteractionTestUtilInteractiveUiTest() {
    InteractionTestUtilBrowser::PopulateSimulators(test_util_);
    test_util_.AddSimulator(
        std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
  }
  ~WebContentsInteractionTestUtilInteractiveUiTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ui::test::InteractionTestUtil test_util_;
};


// This test checks that we can attach to a WebUI that isn't embedded in a tab.
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilInteractiveUiTest,
                       OpenTabSearchMenuAndAccessWebUI) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  std::unique_ptr<WebContentsInteractionTestUtil> tab_search_page;
  const auto context = BrowserElements::From(browser())->GetContext();

  // Poke into the doc to find something that's not at the top level, just to
  // verify we can.
  const WebContentsInteractionTestUtil::DeepQuery kTabSearchListQuery = {
      "tab-search-app", "tab-search-page"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* seq,
                               ui::TrackedElement* element) {
                             if (test_util_.PressButton(element) !=
                                 ui::test::ActionResult::kSucceeded) {
                               seq->FailForTesting();
                             }
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kTabSearchBubbleElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        auto* const web_view =
                            views::AsViewClass<WebUIBubbleDialogView>(
                                element->AsA<views::TrackedElementViews>()
                                    ->view())
                                ->web_view();
                        tab_search_page =
                            WebContentsInteractionTestUtil::ForNonTabWebView(
                                web_view, kTabSearchPageElementId);
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test checks that when a WebUI is hidden, its element goes away.
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilInteractiveUiTest,
                       OpenTabSearchMenuAndTestVisibility) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  std::unique_ptr<WebContentsInteractionTestUtil> tab_search_page;
  const auto context = BrowserElements::From(browser())->GetContext();
  raw_ptr<WebUIBubbleDialogView> bubble_view = nullptr;

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(context)
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchButtonElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* seq,
                               ui::TrackedElement* element) {
                             if (test_util_.PressButton(element) !=
                                 ui::test::ActionResult::kSucceeded) {
                               seq->FailForTesting();
                             }
                           }))
                       .Build())
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetType(ui::InteractionSequence::StepType::kShown)
                  .SetElementID(kTabSearchBubbleElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        bubble_view = views::AsViewClass<WebUIBubbleDialogView>(
                            element->AsA<views::TrackedElementViews>()->view());
                        tab_search_page =
                            WebContentsInteractionTestUtil::ForNonTabWebView(
                                bubble_view->web_view(),
                                kTabSearchPageElementId);
                      }))
                  .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Hide the ancestor view. This should hide the
                             // whole chain and cause the element to be
                             // destroyed.
                             bubble_view->SetVisible(false);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementID(kTabSearchPageElementId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence* sequence,
                               ui::TrackedElement* element) {
                             // Verify we've also disposed of the element
                             // itself:
                             EXPECT_EQ(nullptr,
                                       tab_search_page->current_element_);
                             // Show the ancestor view. This should recreate the
                             // WebUI element.
                             bubble_view->SetVisible(true);
                           }))
                       .Build())
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kShown)
                       .SetElementID(kTabSearchPageElementId)
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test verifies that the bounds of an element can be retrieved.
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilInteractiveUiTest,
                       GetElementBoundsInScreen) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithButtonURL);
  auto page = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const WebContentsInteractionTestUtil::DeepQuery kButtonQuery = {"#button"};
  const WebContentsInteractionTestUtil::DeepQuery kTextQuery = {"#text"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(BrowserElements::From(browser())->GetContext())
          // Navigate to the test page.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [url](ui::InteractionSequence*,
                            ui::TrackedElement* element) {
                        auto* const owner =
                            element->AsA<TrackedElementWebContents>()->owner();
                        owner->LoadPage(url);
                      }))
                  .Build())
          // Wait for the navigation to complete and check the button bounds.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kWebContentsElementId)
                  .SetTransitionOnlyOnEvent(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&, url](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                        auto* const owner =
                            element->AsA<TrackedElementWebContents>()->owner();
                        ASSERT_EQ(url, owner->web_contents()->GetURL());

                        // Check the button bounds.
                        const gfx::Rect element_rect =
                            owner->GetElementBoundsInScreen(kButtonQuery);
                        EXPECT_FALSE(element_rect.IsEmpty());
                        const gfx::Rect window_rect =
                            browser()->window()->GetBounds();
                        EXPECT_TRUE(window_rect.Contains(element_rect))
                            << "Expected window rect " << window_rect.ToString()
                            << " to contain element rect "
                            << element_rect.ToString();

                        // Verify that the text element *is* empty.
                        const gfx::Rect text_rect =
                            owner->GetElementBoundsInScreen(kTextQuery);
                        EXPECT_TRUE(text_rect.IsEmpty());
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// This test verifies that the bounds of an element can be retrieved.
IN_PROC_BROWSER_TEST_F(WebContentsInteractionTestUtilInteractiveUiTest,
                       GetIframeElementBoundsInScreen) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL url = embedded_test_server()->GetURL(kDocumentWithIframe);
  auto page = WebContentsInteractionTestUtil::ForExistingTabInBrowser(
      browser(), kWebContentsElementId);
  const WebContentsInteractionTestUtil::DeepQuery kContainerQuery = {
      "#container"};
  const WebContentsInteractionTestUtil::DeepQuery kIframeQuery = {"#iframe"};
  const WebContentsInteractionTestUtil::DeepQuery kTopElementQuery = {"#iframe",
                                                                      "p"};
  const WebContentsInteractionTestUtil::DeepQuery kLinkQuery = {"#iframe",
                                                                "#ref"};

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(BrowserElements::From(browser())->GetContext())
          // Navigate to the test page.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kWebContentsElementId)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [url](ui::InteractionSequence*,
                            ui::TrackedElement* element) {
                        auto* const owner =
                            element->AsA<TrackedElementWebContents>()->owner();
                        owner->LoadPage(url);
                      }))
                  .Build())
          // Wait for the navigation to complete and check the button bounds.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(kWebContentsElementId)
                  .SetTransitionOnlyOnEvent(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&, url](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                        auto* const owner =
                            element->AsA<TrackedElementWebContents>()->owner();
                        const gfx::Rect window_rect =
                            browser()->window()->GetBounds();
                        const gfx::Rect container_rect =
                            owner->GetElementBoundsInScreen(kContainerQuery);

                        const gfx::Rect iframe_rect =
                            owner->GetElementBoundsInScreen(kIframeQuery);
                        EXPECT_FALSE(iframe_rect.IsEmpty());
                        EXPECT_TRUE(window_rect.Contains(iframe_rect))
                            << "Expected window rect " << window_rect.ToString()
                            << " to contain element rect "
                            << iframe_rect.ToString();
                        EXPECT_TRUE(container_rect.Contains(iframe_rect))
                            << "Expected container rect "
                            << container_rect.ToString()
                            << " to contain element rect "
                            << iframe_rect.ToString();

                        const gfx::Rect top_element_rect =
                            owner->GetElementBoundsInScreen(kTopElementQuery);
                        EXPECT_FALSE(top_element_rect.IsEmpty());
                        EXPECT_TRUE(iframe_rect.Contains(top_element_rect))
                            << "Expected iframe rect " << iframe_rect.ToString()
                            << " to contain element rect "
                            << top_element_rect.ToString();

                        const gfx::Rect link_rect =
                            owner->GetElementBoundsInScreen(kLinkQuery);
                        EXPECT_FALSE(link_rect.IsEmpty());
                        EXPECT_TRUE(iframe_rect.Contains(link_rect))
                            << "Expected iframe rect " << iframe_rect.ToString()
                            << " to contain element rect "
                            << link_rect.ToString();
                        EXPECT_GT(link_rect.y(), top_element_rect.y())
                            << "Expected element rect " << link_rect.ToString()
                            << " to be strictly lower on the page than the top "
                               "element "
                            << top_element_rect.ToString();
                      }))
                  .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
