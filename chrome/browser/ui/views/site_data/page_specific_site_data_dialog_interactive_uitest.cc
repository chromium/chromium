// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"
#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog_controller.h"
#include "chrome/browser/ui/views/site_data/site_data_row_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/webui_interaction_test_util.h"
#include "components/page_info/core/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_sequence_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebUIInteractionTestUtilTestId);
const char kFirstPartyAllowedRow[] = "FirstPartyAllowedRow";
const char kThirdPartyBlockedRow[] = "ThirdPartyBlockedRow";
const char kOnlyPartitionedRow[] = "OnlyPartitionedRow";
const char kMixedPartitionedRow[] = "MixedPartitionedRow";
const char kCookiesDialogHistogramName[] = "Privacy.CookiesInUseDialog.Action";

void ExpectActionCount(const base::HistogramTester& histograms,
                       PageSpecificSiteDataDialogAction action,
                       int count) {
  histograms.ExpectBucketCount(kCookiesDialogHistogramName,
                               static_cast<int>(action), count);
}

}  // namespace

class PageSpecificSiteDataDialogInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  PageSpecificSiteDataDialogInteractiveUiTest() {
    feature_list_.InitWithFeatures(
        {page_info::kPageSpecificSiteDataDialog,
         net::features::kPartitionedCookies,
         net::features::kPartitionedCookiesBypassOriginTrial},
        {});
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
  }

  ~PageSpecificSiteDataDialogInteractiveUiTest() override = default;
  PageSpecificSiteDataDialogInteractiveUiTest(
      const PageSpecificSiteDataDialogInteractiveUiTest&) = delete;
  void operator=(const PageSpecificSiteDataDialogInteractiveUiTest&) = delete;

  void SetUp() override {
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->ServeFilesFromSourceDirectory(GetChromeTestDataDir());

    set_open_about_blank_on_browser_launch(true);

    ASSERT_TRUE(https_server()->InitializeAndListen());
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
    content::SetupCrossSiteRedirector(https_server());
    https_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  // Helper function that wait for element to show and click on it.
  // This function will be implemented by a test util framework later.
  auto Click(ui::ElementIdentifier element_id) {
    return ui::InteractionSequence::StepBuilder()
        .SetFindElementInAnyContext(true)
        .SetElementID(element_id)
        .SetMustRemainVisible(false)
        .SetStartCallback(base::BindLambdaForTesting(
            [&](ui::InteractionSequence*, ui::TrackedElement* element) {
              test_util_.PressButton(element);
            }))
        .Build();
  }

  auto CheckIsElementPresent(ui::ElementIdentifier element_id) {
    return ui::InteractionSequence::StepBuilder()
        .SetFindElementInAnyContext(true)
        .SetElementID(element_id)
        .Build();
  }

  ui::TrackedElement* GetElementByIdentifier(ui::ElementContext context,
                                             ui::ElementIdentifier id) {
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        id, context);
  }

  ui::ElementContext GetContext(ui::TrackedElement* element) {
    return views::ElementTrackerViews::GetContextForView(
        element->AsA<views::TrackedElementViews>()->view());
  }

  std::u16string GetStateLabelText(ui::TrackedElement* row_element) {
    auto* const row_view = views::AsViewClass<SiteDataRowView>(
        row_element->AsA<views::TrackedElementViews>()->view());
    return row_view->state_label_for_testing()->GetText();
  }

  ui::TrackedElement* GetMenuButtonElement(ui::TrackedElement* row_element) {
    auto* const row_view = views::AsViewClass<SiteDataRowView>(
        row_element->AsA<views::TrackedElementViews>()->view());
    return views::ElementTrackerViews::GetInstance()->GetElementForView(
        row_view->menu_button_for_testing());
  }

  ui::TrackedElement* GetDeleteButtonElement(ui::TrackedElement* row_element) {
    auto* const row_view = views::AsViewClass<SiteDataRowView>(
        row_element->AsA<views::TrackedElementViews>()->view());
    return views::ElementTrackerViews::GetInstance()->GetElementForView(
        row_view->delete_button_for_testing());
  }

  ui::test::InteractionTestUtil test_util_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       FirstPartyAllowed) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);

  const GURL third_party_cookie_page_url =
      https_server()->GetURL("a.test", "/third_party_partitioned_cookies.html");

  Browser* browser = CreateIncognitoBrowser();

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser, kWebUIInteractionTestUtilTestId);
  util->LoadPage(third_party_cookie_page_url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser->window()->GetElementContext())
          // Wait for page loaded.
          .AddStep(ui::InteractionSequence::StepBuilder().SetElementID(
              kWebUIInteractionTestUtilTestId))
          .AddStep(Click(kLocationIconElementId))
          .AddStep(Click(PageInfoMainView::kCookieButtonElementId))
          // Verify the dialog was opened.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogFirstPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kDialogOpened, 1);
                      })))
          // Find the first party section and name the first row.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogFirstPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        auto* const section_view =
                            element->AsA<views::TrackedElementViews>()->view();
                        auto* const row_view = section_view->children()[0];
                        views::InteractionSequenceViews::NameView(
                            sequence, row_view, kFirstPartyAllowedRow);

                        // Verify that the empty state label isn't present.
                        EXPECT_EQ(
                            nullptr,
                            GetElementByIdentifier(
                                GetContext(element),
                                kPageSpecificSiteDataDialogEmptyStateLabel));
                      })))
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kFirstPartyAllowedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));
                        test_util_.PressButton(GetMenuButtonElement(element));
                      })))
          // Verify that the menu has "Block" and "Clear on exit" menu
          // items.
          .AddStep(CheckIsElementPresent(SiteDataRowView::kBlockMenuItem))
          .AddStep(CheckIsElementPresent(SiteDataRowView::kClearOnExitMenuItem))
          // Verify that the site can be deleted.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementName(kFirstPartyAllowedRow)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             // Verify that the menu doesn't have "Allow" menu
                             // item because it is already allowed.
                             EXPECT_EQ(nullptr,
                                       GetElementByIdentifier(
                                           GetContext(element),
                                           SiteDataRowView::kAllowMenuItem));
                             test_util_.PressButton(
                                 GetDeleteButtonElement(element));
                           })))
          // Verify that UI has updated as a result of clicking on the button
          // and the correct histogram was logged.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementName(kFirstPartyAllowedRow)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             ExpectActionCount(
                                 histograms,
                                 PageSpecificSiteDataDialogAction::kSiteDeleted,
                                 1);
                           })))
          // Verify that after deleting the last (and only) row in a section, a
          // label explaining the empty state is shown.
          .AddStep(
              CheckIsElementPresent(kPageSpecificSiteDataDialogEmptyStateLabel))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       ThirdPartyBlocked) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);

  const GURL third_party_cookie_page_url =
      https_server()->GetURL("a.test", "/third_party_partitioned_cookies.html");

  Browser* browser = CreateIncognitoBrowser();

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser, kWebUIInteractionTestUtilTestId);
  util->LoadPage(third_party_cookie_page_url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser->window()->GetElementContext())
          // Wait for page loaded.
          .AddStep(ui::InteractionSequence::StepBuilder().SetElementID(
              kWebUIInteractionTestUtilTestId))
          .AddStep(Click(kLocationIconElementId))
          .AddStep(Click(PageInfoMainView::kCookieButtonElementId))
          // Verify the dialog was opened.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogFirstPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kDialogOpened, 1);
                      })))
          // Find the third party section and name the third-party cookies row
          // (from d.test).
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogThirdPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        auto* const section_view =
                            element->AsA<views::TrackedElementViews>()->view();
                        auto* const row_view = section_view->children()[2];
                        views::InteractionSequenceViews::NameView(
                            sequence, row_view, kThirdPartyBlockedRow);
                      })))
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kThirdPartyBlockedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
                        test_util_.PressButton(GetMenuButtonElement(element));
                      })))
          // Verify that the menu has "Clear on exit" and "Allow" menu items.
          .AddStep(CheckIsElementPresent(SiteDataRowView::kClearOnExitMenuItem))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetFindElementInAnyContext(true)
                       .SetElementID(SiteDataRowView::kAllowMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             // Verify that the menu doesn't have "Block" or
                             // "Delete" menu items because it is already
                             // blocked.
                             EXPECT_EQ(nullptr,
                                       GetElementByIdentifier(
                                           GetContext(element),
                                           SiteDataRowView::kBlockMenuItem));

                             test_util_.SelectMenuItem(element);
                           })))
          // Wait until custom event happens (triggered when any menu item
          // callback is called). Menu item is accepted on Mac async, after
          // closure animation finished.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kSiteRowMenuItemClicked)
                       .SetElementName(kThirdPartyBlockedRow))
          // Verify that UI has updated as a result of clicking on a menu item
          // and the correct histogram was logged. Open the content menu again.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kThirdPartyBlockedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kSiteAllowed, 1);
                      })))
          // Verify that after allowing a site, it can be deleted.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementName(kThirdPartyBlockedRow)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util_.PressButton(
                                 GetDeleteButtonElement(element));
                           })))
          // Verify that UI has updated as a result of clicking on the delete
          // button and the correct histogram was logged.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kHidden)
                       .SetElementName(kThirdPartyBlockedRow)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             ExpectActionCount(
                                 histograms,
                                 PageSpecificSiteDataDialogAction::kSiteDeleted,
                                 1);
                           })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       OnlyPartitionedBlockedThirdPartyCookies) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);

  const GURL third_party_cookie_page_url =
      https_server()->GetURL("a.test", "/third_party_partitioned_cookies.html");

  Browser* browser = CreateIncognitoBrowser();

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser, kWebUIInteractionTestUtilTestId);
  util->LoadPage(third_party_cookie_page_url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser->window()->GetElementContext())
          // Wait for page loaded.
          .AddStep(ui::InteractionSequence::StepBuilder().SetElementID(
              kWebUIInteractionTestUtilTestId))
          .AddStep(Click(kLocationIconElementId))
          .AddStep(Click(PageInfoMainView::kCookieButtonElementId))
          // Verify the dialog was opened.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogFirstPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kDialogOpened, 1);
                      })))
          // Find the third party section and name the row with partitioned only
          // access (b.test).
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogThirdPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        auto* const section_view =
                            element->AsA<views::TrackedElementViews>()->view();
                        auto* const partitioned_row_view =
                            section_view->children()[0];
                        views::InteractionSequenceViews::NameView(
                            sequence, partitioned_row_view,
                            kOnlyPartitionedRow);
                      })))
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kOnlyPartitionedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE));
                        test_util_.PressButton(GetMenuButtonElement(element));
                      })))
          // Verify that the menu has "Clear on exit", "Allow" and "Block" menu
          // items. Even though the website didn't try to access third party
          // cookies, the allow option is still offered for consistency.
          .AddStep(CheckIsElementPresent(SiteDataRowView::kClearOnExitMenuItem))
          .AddStep(CheckIsElementPresent(SiteDataRowView::kAllowMenuItem))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetFindElementInAnyContext(true)
                       .SetElementID(SiteDataRowView::kBlockMenuItem)
                       .SetMustRemainVisible(false)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             test_util_.SelectMenuItem(element);
                           })))
          // Wait until custom event happens (triggered when any menu item
          // callback is called). Menu item is accepted on Mac async, after
          // closure animation finished.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kSiteRowMenuItemClicked)
                       .SetElementName(kOnlyPartitionedRow))
          // Verify that UI has updated as a result of clicking on a menu item
          // and the correct histogram was logged.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kOnlyPartitionedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_BLOCKED_STATE_SUBTITLE));
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kSiteBlocked, 1);
                      })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

IN_PROC_BROWSER_TEST_F(PageSpecificSiteDataDialogInteractiveUiTest,
                       MixedPartitionedBlockedThirdPartyCookies) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kCookiesDialogHistogramName, 0);

  const GURL third_party_cookie_page_url =
      https_server()->GetURL("a.test", "/third_party_partitioned_cookies.html");

  Browser* browser = CreateIncognitoBrowser();

  auto util = WebUIInteractionTestUtil::ForExistingTabInBrowser(
      browser, kWebUIInteractionTestUtilTestId);
  util->LoadPage(third_party_cookie_page_url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser->window()->GetElementContext())
          // Wait for page loaded.
          .AddStep(ui::InteractionSequence::StepBuilder().SetElementID(
              kWebUIInteractionTestUtilTestId))
          .AddStep(Click(kLocationIconElementId))
          .AddStep(Click(PageInfoMainView::kCookieButtonElementId))
          // Verify the dialog was opened.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogFirstPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kDialogOpened, 1);
                      })))
          // Find the third party section and name the row with mixed storage
          // access (c.test).
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(kPageSpecificSiteDataDialogThirdPartySection)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence* sequence,
                          ui::TrackedElement* element) {
                        auto* const section_view =
                            element->AsA<views::TrackedElementViews>()->view();
                        auto* const mixed_row_view =
                            section_view->children()[1];
                        views::InteractionSequenceViews::NameView(
                            sequence, mixed_row_view, kMixedPartitionedRow);
                      })))
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kMixedPartitionedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_PARTITIONED_STATE_SUBTITLE));
                        test_util_.PressButton(GetMenuButtonElement(element));
                      })))
          // Verify that the menu has "Clear on exit", "Allow" and "Block" menu
          // items.
          .AddStep(CheckIsElementPresent(SiteDataRowView::kClearOnExitMenuItem))
          .AddStep(CheckIsElementPresent(SiteDataRowView::kBlockMenuItem))
          // "Allow" menu item is shown because the site has access 3PC and
          // they were blocked. "Allow" menu item is here to allow 3PC access.
          // It also has special string that specifies allowing 3PC.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetFindElementInAnyContext(true)
                  .SetElementID(SiteDataRowView::kAllowMenuItem)
                  .SetMustRemainVisible(false)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        auto* const menu_item_view =
                            views::AsViewClass<views::MenuItemView>(
                                element->AsA<views::TrackedElementViews>()
                                    ->view());
                        EXPECT_EQ(
                            menu_item_view->title(),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOW_THIRD_PARTY_MENU_ITEM));
                        test_util_.SelectMenuItem(element);
                      })))
          // Wait until custom event happens (triggered when any menu item
          // callback is called). Menu item is accepted on Mac async, after
          // closure animation finished.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kSiteRowMenuItemClicked)
                       .SetElementName(kMixedPartitionedRow))
          // Verify that UI has updated as a result of clicking on a menu item
          // and the correct histogram was logged.
          .AddStep(
              ui::InteractionSequence::StepBuilder()
                  .SetElementName(kMixedPartitionedRow)
                  .SetStartCallback(base::BindLambdaForTesting(
                      [&](ui::InteractionSequence*,
                          ui::TrackedElement* element) {
                        EXPECT_EQ(
                            GetStateLabelText(element),
                            l10n_util::GetStringUTF16(
                                IDS_PAGE_SPECIFIC_SITE_DATA_DIALOG_ALLOWED_STATE_SUBTITLE));
                        ExpectActionCount(
                            histograms,
                            PageSpecificSiteDataDialogAction::kSiteAllowed, 1);
                      })))
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}
