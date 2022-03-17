// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_util.h"

class ContextMenuIncognitoFilterBrowserTest : public InProcessBrowserTest {
 public:
  constexpr static const char kCrossOtrResponseMetricName[] =
      "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";
  constexpr static const char kCrossOtrRefreshCountMetricName[] =
      "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental";

  void SetUpInProcessBrowserTestFixture() override {
    // Enable open in incognito param filtering, with rules for:
    // a destination of: <IP address>, for which eTLD+1 is blank,
    // with outgoing param plzblock
    // or a source of: foo.com with outgoing param plzblock1
    std::string encoded_classification = url_param_filter::
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{"foo.com", {"plzblock1"}}}, {{"", {"plzblock"}}});
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterBrowserTest,
                       OpenIncognitoUrlParamFilter) {
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?plzblock=1&nochanges=2&plzblock1=2"));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kSource;
  context_menu_params.link_url = test_root;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      *browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the filtered URL.
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 200, and the navigation went from normal-->OTR browsing.
  histogram_tester.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

// Verify that appropriate metrics are written when redirects are encountered.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterBrowserTest,
                       OpenIncognitoUrlParamFilterRedirect) {
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL("/defaultresponse"));
  GURL redirect_page(
      embedded_test_server()->GetURL("/server-redirect?" + test_root.spec()));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + redirect_page.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://redirect.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kSource;
  context_menu_params.link_url = redirect_page;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      *browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the filtered URL.
  GURL expected(embedded_test_server()->GetURL("/defaultresponse"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // Navigate elsewhere and ensure we don't get additional metrics.
  content::OpenURLParams params(test_root, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  content::WebContents* second_contents = tab->OpenURL(params);
  EXPECT_TRUE(content::WaitForLoadStop(second_contents));
  // The response was a 301 redirect followed by a 200, and the navigation went
  // from normal-->OTR browsing. The later OpenURL should not write a metric.
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(301), 1);
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

// Ensure that enabling URL param filtering does not apply to "Open in new tab"
// and that cross-off-the-record metrics are not written in that case.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterBrowserTest,
                       OpenTabNoUrlParamFilter) {
  const char kPath[] = "/empty.html?plzblock=1&nochanges=2&plzblock1=2";
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(kPath));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kSource;
  context_menu_params.link_url = test_root;

  // Select "Open Link in New Tab" and wait for the tab to be added.
  TestRenderViewContextMenu menu(
      *browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  content::WebContents* tab = tab_added_waiter.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the original URL; open in new tab should not filter.
  GURL expected(embedded_test_server()->GetURL(kPath));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // Ensure we did not erroneously record a cross-off-the-record metric; the
  // navigation did not cross over.
  histogram_tester.ExpectTotalCount(kCrossOtrResponseMetricName, 0);
}

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it does not filter when it is not configured to do so.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterBrowserTest,
                       OpenIncognitoUrlParamFilterWithoutChanges) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?noblock=1&nochanges=2&alsonoblock=2"));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kSource;
  context_menu_params.link_url = test_root;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      *browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the unfiltered URL.
  ASSERT_EQ(test_root, tab->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterBrowserTest,
                       CrossOtrRefreshCount) {
  base::HistogramTester histogram_tester;
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?noblock=1&nochanges=2&alsonoblock=2"));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kSource;
  context_menu_params.link_url = test_root;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      *browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the unfiltered URL.
  ASSERT_EQ(test_root, tab->GetLastCommittedURL());

  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Navigate elsewhere and ensure we don't get additional metrics.
  GURL second_nav = GURL(embedded_test_server()->GetURL("/defaultresponse"));
  content::OpenURLParams params(second_nav, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);

  content::WebContents* second_contents = tab->OpenURL(params);
  EXPECT_TRUE(content::WaitForLoadStop(second_contents));
  // Refresh post-second navigation. This is not a cross-OTR-relevant refresh.
  second_contents->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(second_contents));

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 1);
}

IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterBrowserTest,
                       CrossOtrRefreshCountDestroyedContents) {
  base::HistogramTester histogram_tester;
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?noblock=1&nochanges=2&alsonoblock=2"));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

  // Set up menu with link URL.
  content::ContextMenuParams context_menu_params;
  context_menu_params.page_url = kSource;
  context_menu_params.link_url = test_root;

  // Select "Open Link in Incognito Window" and wait for window to be added.
  TestRenderViewContextMenu menu(
      *browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the unfiltered URL.
  ASSERT_EQ(test_root, tab->GetLastCommittedURL());

  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Destroy the web contents and ensure we don't lose the refresh count metric.
  tab->Close();

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 1);
}
