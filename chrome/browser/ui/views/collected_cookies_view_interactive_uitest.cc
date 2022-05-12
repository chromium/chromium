// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_sequence_browser_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInteractionSequenceBrowserUtilTestId);
}  // namespace

class CollectedCookiesViewInteractiveUiTest : public InProcessBrowserTest {
 public:
  CollectedCookiesViewInteractiveUiTest() = default;
  ~CollectedCookiesViewInteractiveUiTest() override = default;
  CollectedCookiesViewInteractiveUiTest(
      const CollectedCookiesViewInteractiveUiTest&) = delete;
  void operator=(const CollectedCookiesViewInteractiveUiTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    test_util_.AddSimulator(
        std::make_unique<views::test::InteractionTestUtilSimulatorViews>());
#if BUILDFLAG(IS_MAC)
    test_util_.AddSimulator(
        std::make_unique<ui::test::InteractionTestUtilSimulatorMac>());
#endif
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Helper function that wait for element to show and click on it.
  // This function will be implemented by a test util framework later.
  auto Click(ui::ElementIdentifier element_id) {
    return ui::InteractionSequence::StepBuilder()
        .SetElementID(element_id)
        .SetMustRemainVisible(false)
        .SetStartCallback(base::BindLambdaForTesting(
            [&](ui::InteractionSequence*, ui::TrackedElement* element) {
              test_util_.PressButton(element);
            }))
        .Build();
  }

  ui::test::InteractionTestUtil test_util_;
};

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewInteractiveUiTest,
                       CheckCookiesAreBlockedInIncognitoWindow) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL third_party_cookie_page_url =
      embedded_test_server()->GetURL("a.com", "/third_party_cookie.html");

  Browser* browser = CreateIncognitoBrowser();

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser, kInteractionSequenceBrowserUtilTestId);
  util->LoadPage(third_party_cookie_page_url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser->window()->GetElementContext())
          // Wait for page loaded.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .Build())
          .AddStep(Click(kLocationIconElementId))
          .AddStep(Click(PageInfoMainView::kCookieButtonElementId))
          // Switch to Cookies in use - Blocked tab.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementID(CollectedCookiesViews::kTabbedPaneElementId)
                  .SetFindElementInAnyContext(true)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        // Tab on Blocked tab.
                        views::AsViewClass<views::TabbedPane>(
                            element->AsA<views::TrackedElementViews>()->view())
                            ->SelectTabAt(1);
                      }))
                  .Build())
          // Verify the blocked cookies.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(
                           CollectedCookiesViews::kBlockedCookiesTreeElementId)
                       .SetFindElementInAnyContext(true)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             auto* blocked_cookies_tree =
                                 views::AsViewClass<views::TreeView>(
                                     element->AsA<views::TrackedElementViews>()
                                         ->view());
                             ASSERT_EQ(1, blocked_cookies_tree->GetRowCount());

                             auto* model = blocked_cookies_tree->model();
                             auto nodes = model->GetChildren(model->GetRoot());
                             ASSERT_EQ(1u, nodes.size());
                             ASSERT_EQ("http://b.com",
                                       static_cast<CookieTreeNode*>(nodes[0])
                                           ->GetDetailedInfo()
                                           .origin.Serialize());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
