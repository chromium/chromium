// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/page_info/page_info_main_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/page_info/core/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/controls/tree/tree_view.h"
#include "ui/views/view_utils.h"

class CollectedCookiesViewInteractiveUiTest : public InteractiveBrowserTest {
 public:
  CollectedCookiesViewInteractiveUiTest() {
    // TODO(crbug.com/1344787): Clean up when PageSpecificSiteDataDialog is
    // launched. Disable features for the new version of "Cookies in use"
    // dialog. These tests are for the current version of the dialog only.
    feature_list_.InitWithFeatures({}, {page_info::kPageSpecificSiteDataDialog,
                                        page_info::kPageInfoCookiesSubpage});
  }

  ~CollectedCookiesViewInteractiveUiTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CollectedCookiesViewInteractiveUiTest,
                       // TODO(crbug.com/1417914): Re-enable this test
                       DISABLED_CheckCookiesAreBlockedInIncognitoWindow) {
  const GURL third_party_cookie_page_url =
      embedded_test_server()->GetURL("a.com", "/third_party_cookie.html");

  Browser* incognito_browser = CreateIncognitoBrowser();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebPageElementId);

  RunTestSequenceInContext(
      incognito_browser->window()->GetElementContext(),
      InstrumentTab(kWebPageElementId),
      NavigateWebContents(kWebPageElementId, third_party_cookie_page_url),
      PressButton(kLocationIconElementId),
      PressButton(PageInfoMainView::kCookieButtonElementId),
      // Select "blocked" tab.
      InAnyContext(SelectTab(CollectedCookiesViews::kTabbedPaneElementId, 1)),
      InAnyContext(AfterShow(
          CollectedCookiesViews::kBlockedCookiesTreeElementId,
          base::BindLambdaForTesting([&](ui::TrackedElement* element) {
            auto* blocked_cookies_tree = AsView<views::TreeView>(element);
            ASSERT_EQ(1u, blocked_cookies_tree->GetRowCount());

            auto* model = blocked_cookies_tree->model();
            auto nodes = model->GetChildren(model->GetRoot());
            ASSERT_EQ(1u, nodes.size());
            ASSERT_EQ("http://b.com", static_cast<CookieTreeNode*>(nodes[0])
                                          ->GetDetailedInfo()
                                          .origin.Serialize());
          }))));
}
