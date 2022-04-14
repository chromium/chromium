// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/url_param_filter/url_param_filter_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/escape.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace url_param_filter {

namespace {

constexpr static const char kCrossOtrResponseMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";
constexpr static const char kCrossOtrRefreshCountMetricName[] =
    "Navigation.CrossOtr.ContextMenu.RefreshCountExperimental";
constexpr static const char kFilteredParamCountMetricName[] =
    "Navigation.UrlParamFilter.FilteredParamCountExperimental";

class ContextMenuIncognitoFilterDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // Enable open in incognito param filtering, with rules for:
    // a destination of: <IP address>, for which eTLD+1 is blank,
    // with outgoing param plzblock
    // or a source of: foo.com with outgoing param plzblock1
    std::string encoded_classification = url_param_filter::
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{"foo.com", {"plzblock1"}}, {"127.0.0.1", {"plzblockredirect"}}},
            {{"127.0.0.1", {"plzblock"}}});
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification},
         {"should_filter", "false"}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Disable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that no params are filtered as expected.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterDisabledBrowserTest,
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

  // Verify that the loaded URL has not been filtered.
  GURL expected(embedded_test_server()->GetURL(
      "/empty.html?plzblock=1&nochanges=2&plzblock1=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 200 (params filtered, but no intervention => metrics
  // collected), and the navigation went from normal --> OTR browsing. Since we
  // didn't intervened, we don't expect to see a 307.
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(307), 0);
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

class ContextMenuIncognitoFilterEnabledBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // Enable open in incognito param filtering, with rules for:
    // a destination of: <IP address>, for which eTLD+1 is blank,
    // with outgoing param plzblock
    // or a source of: foo.com with outgoing param plzblock1
    std::string encoded_classification = url_param_filter::
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{"foo.com", {"plzblock1"}}, {"127.0.0.1", {"plzblockredirect"}}},
            {{"127.0.0.1", {"plzblock"}}});
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification},
         {"should_filter", "true"}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
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

  // The response was a 200 (params filtered => metrics collected), and the
  // navigation went from normal --> OTR browsing.
  histogram_tester.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters only main frame navigations.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       OpenIncognitoUrlParamFilterSubresources) {
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/url_param_filter/test.html?plzblock=1&nochanges=2&plzblock1=2"));

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
  GURL expected(embedded_test_server()->GetURL(
      "/url_param_filter/test.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // Ensure that we don't apply filters to subresource requests, even when
  // there's a destination rule for the domain/param pair.
  EXPECT_EQ(true, content::EvalJs(tab,
                                  "document.getElementById('dummy-frame')."
                                  "src.endsWith('plzblock=1')"));
  EXPECT_EQ(true, content::EvalJs(tab,
                                  "document.getElementById('dummy-script').src."
                                  "endsWith('plzblock=1')"));

  // The response was a 200, and the navigation went from normal-->OTR
  // browsing.
  // Ensure we only see the two params on the main navigation being filtered;
  // the other plzblock instances are on js or subframe requests, so should
  // not be filtered.
  EXPECT_EQ(histogram_tester.GetTotalSum(kFilteredParamCountMetricName), 2);
  histogram_tester.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected.
IN_PROC_BROWSER_TEST_F(
    ContextMenuIncognitoFilterEnabledBrowserTest,
    OpenIncognitoUrlParamFilterClientRedirectAfterActivation) {
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

  // Trigger user activation, at which point client redirects are no longer
  // protected.
  content::SimulateMouseClick(tab, 0, blink::WebMouseEvent::Button::kLeft);
  content::LoadStopObserver client_redirect_load_observer(tab);
  std::string script = "document.location.href=\"" + test_root.spec() + "\"";
  ASSERT_TRUE(content::ExecJs(tab, script));
  client_redirect_load_observer.Wait();
  ASSERT_EQ(test_root, tab->GetLastCommittedURL());
}

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected when server redirects are encountered.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       OpenIncognitoUrlParamFilterServerRedirect) {
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  // `plzblockredirect` is blocked on navigations from IP source domains;
  // `plzblock` is blocked when an IP is the destination domain.
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?plzblock=1&nochanges=2&plzblockredirect=2"));
  GURL redirect_page(embedded_test_server()->GetURL(
      "/server-redirect?" +
      net::EscapeQueryParamValue(test_root.spec(), false)));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

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
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 301-->200, and the navigation went from normal-->OTR
  // browsing.
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(301), 1);
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester.ExpectTotalCount(kCrossOtrResponseMetricName, 2);
}

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected when client redirects are encountered.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       OpenIncognitoUrlParamFilterClientRedirect) {
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  // `plzblock1` is blocked only on navs from foo.com. Because analysis will
  // see this as a separate navigation, the source domain of the client
  // redirect will be localhost.
  GURL test_root(
      embedded_test_server()->GetURL("/empty.html?plzblock=1&nochanges=2"));
  GURL redirect_page(embedded_test_server()->GetURL(
      "/client-redirect?" +
      net::EscapeQueryParamValue(test_root.spec(), false)));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

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
  // The prior load stop succeeds for the initial response; we now wait for
  // the client redirect to occur.
  content::LoadStopObserver client_redirect_load_observer(tab);
  client_redirect_load_observer.Wait();
  // Verify that it loaded the filtered URL.
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 200 (pre params filtering => no metrics collected) -->
  // client redirect --> 200 (params filtered => metrics colleted), and the
  // navigation went from normal --> OTR browsing.
  histogram_tester.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
}

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected when client redirects are encountered.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       OpenIncognitoUrlParamFilterClientRedirectThenRefresh) {
  base::HistogramTester histogram_tester;

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(
      embedded_test_server()->GetURL("/empty.html?plzblock=1&nochanges=2"));
  GURL redirect_page(embedded_test_server()->GetURL(
      "/client-redirect?" +
      net::EscapeQueryParamValue(test_root.spec(), false)));

  // Go to a |page| with a link to a URL that has associated filtering rules.
  GURL page("data:text/html,<a href='" + test_root.spec() + "'>link</a>");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page));

  // Set up the source URL to an eTLD+1 that also has a filtering rule.
  const GURL kSource("http://foo.com/test");

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
  // The prior load stop succeeds for the initial response; we now wait for
  // the client redirect to occur.
  content::LoadStopObserver client_redirect_load_observer(tab);
  client_redirect_load_observer.Wait();
  // Verify that it loaded the filtered URL.
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  tab->Close();

  // The response was a 200 (pre params filtering => no metrics collected) -->
  // client redirect --> 200 (params filtered => metrics collected), and the
  // navigation went from normal-->OTR browsing.
  histogram_tester.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 1);
}

// Verify that appropriate metrics are written when redirects are encountered.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
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

  // The response was a 301 (no params filtered => no metrics collected)
  // redirect followed by a 200 (no params filtered => no metrics collected),
  // and the navigation went from normal-->OTR browsing. The later OpenURL
  // should not write a metric.
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(301), 0);
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 0);
}

// Ensure that enabling URL param filtering does not apply to "Open in new tab"
// and that cross-off-the-record metrics are not written in that case.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
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
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       OpenIncognitoNoParamsFiltering) {
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

  // The response was a 200 (no params filtered => no metrics collected), and
  // the navigation went from normal --> OTR browsing.
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 0);
  histogram_tester.ExpectBucketCount(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(307), 0);
}

IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       CrossOtrRefreshCount) {
  base::HistogramTester histogram_tester;
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?plzblock=1&nochanges=2&alsonoblock=2"));

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
  GURL expected(
      embedded_test_server()->GetURL("/empty.html?nochanges=2&alsonoblock=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // This is a cross-OTR-relevant refresh (params filtered => metrics
  // collected).
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

IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       CrossOtrRefreshCountNoParamsFiltering) {
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

  // This is a cross-OTR-relevant refresh (no params filtered => no metrics
  // collected).
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

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 0);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 0);
}

IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       CrossOtrRefreshCountDestroyedContents) {
  base::HistogramTester histogram_tester;
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(embedded_test_server()->GetURL(
      "/empty.html?plzblock=1&nochanges=2&alsonoblock=2"));

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
  GURL expected(
      embedded_test_server()->GetURL("/empty.html?nochanges=2&alsonoblock=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // This is a cross-OTR-relevant refresh (params filtered => metrics
  // collected).
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Destroy the web contents and ensure we don't lose the refresh count metric.
  tab->Close();

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 1);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 1);
}

IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterEnabledBrowserTest,
                       CrossOtrRefreshCountDestroyedContentsNoParamsFiltering) {
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

  // This is a cross-OTR-relevant refresh (no params filtered => no metrics
  // collected).
  tab->GetController().Reload(content::ReloadType::NORMAL, false);
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Destroy the web contents and ensure we don't lose the refresh count metric.
  tab->Close();

  histogram_tester.ExpectTotalCount(kCrossOtrRefreshCountMetricName, 0);
  ASSERT_EQ(histogram_tester.GetTotalSum(kCrossOtrRefreshCountMetricName), 0);
}

class EnterpriseContextMenuIncognitoFilterBrowserTest
    : public policy::PolicyTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // Enable open in incognito param filtering, with rules for:
    // a destination of: <IP address>, for which eTLD+1 is blank,
    // with outgoing param plzblock
    // or a source of: foo.com with outgoing param plzblock1
    std::string encoded_classification = url_param_filter::
        CreateBase64EncodedFilterParamClassificationForTesting(
            {{"foo.com", {"plzblock1"}}, {"127.0.0.1", {"plzblockredirect"}}},
            {{"127.0.0.1", {"plzblock"}}});

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications", encoded_classification},
         {"should_filter", "true"}});
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    SetPolicy(&policies, policy::key::kUrlParamFilterEnabled,
              absl::optional<base::Value>(false));
    provider_.UpdateChromePolicy(policies);
  }
  // Prevent additional feature/field trial enablement beyond that defined in
  // `SetUpInProcessBrowserTestFixture`.
  void SetUpCommandLine(base::CommandLine* command_line) override {}

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When the policy is disabled, filtering should not occur.
IN_PROC_BROWSER_TEST_F(EnterpriseContextMenuIncognitoFilterBrowserTest,
                       PolicyDisabled) {
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

  // Verify that it loaded the unfiltered URL due to the escape hatch policy
  // overriding the feature flag.
  ASSERT_EQ(test_root, tab->GetLastCommittedURL());
}

// When the policy is enabled, filtering should occur, and changes should not
// require a browser restart.
IN_PROC_BROWSER_TEST_F(EnterpriseContextMenuIncognitoFilterBrowserTest,
                       PolicyEnabledDynamicRefresh) {
  base::HistogramTester histogram_tester;

  // Reset the policy that was already set to false in
  // `SetUpInProcessBrowserTestFixture`, then see if the change is reflected
  // without requiring a browser restart.
  policy::PolicyMap policies;
  SetPolicy(&policies, policy::key::kUrlParamFilterEnabled,
            absl::optional<base::Value>(true));
  provider_.UpdateChromePolicy(policies);

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
}

}  // namespace

}  // namespace url_param_filter
