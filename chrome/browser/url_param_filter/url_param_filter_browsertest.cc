// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include "base/strings/escape.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/component_updater/installer_policies/url_param_classification_component_installer.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/policy_constants.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filter_test_helper.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace url_param_filter {

namespace {

constexpr static const char kCrossOtrResponseMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCode";
constexpr static const char kCrossOtrResponseExperimentalMetricName[] =
    "Navigation.CrossOtr.ContextMenu.ResponseCodeExperimental";
constexpr static const char kCrossOtrRefreshCountMetricName[] =
    "Navigation.CrossOtr.ContextMenu.RefreshCount";
constexpr static const char kFilteredParamCountMetricName[] =
    "Navigation.UrlParamFilter.FilteredParamCount";
constexpr static const char kApplicableSourceClassificationCount[] =
    "Navigation.UrlParamFilter.ApplicableClassificationCount.Source";
constexpr static const char kApplicableDestinationClassificationCount[] =
    "Navigation.UrlParamFilter.ApplicableClassificationCount.Destination";
constexpr static const char kDefaultTag[] = "default";

}  // namespace

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
      base::EscapeQueryParamValue(test_root.spec(), false)));

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
      base::EscapeQueryParamValue(test_root.spec(), false)));

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
      base::EscapeQueryParamValue(test_root.spec(), false)));

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the filtered URL.
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());
}

// Several subclasses must be created in order to test the end-to-end component
// updater + "Open in Incognito" flow, since it relies on the
// kIncognitoParamFilterEnabled feature flag and the UrlParamClassification
// Component, and these must be instantiated within
// SetUpInProcessBrowserTextFixture() rather than from within a test.
class ContextMenuIncognitoFilterComponentUpdaterBrowserTest
    : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled, {{"should_filter", "true"}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Enable open in incognito param filtering, with rules for:
    // a destination of: 127.0.0.1, with outgoing param plzblock
    // or a source of: foo.com with outgoing param plzblock1
    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"foo.com", {"plzblock1"}},
                 {"127.0.0.1", {"plzblockredirect"}}},
                {{"127.0.0.1", {"plzblock"}}}, {kDefaultTag}));
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    // Remove this switch to allow components to be updated.
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }

  void TearDown() override {
    ClassificationsLoader::GetInstance()->ResetListsForTesting();
    component_updater::UrlParamClassificationComponentInstallerPolicy::
        ResetForTesting();
    InProcessBrowserTest::TearDown();
  }
  base::ScopedTempDir component_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected.
IN_PROC_BROWSER_TEST_F(ContextMenuIncognitoFilterComponentUpdaterBrowserTest,
                       OpenIncognitoUrlParamFilter) {
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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
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
  histogram_tester_.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  // All params being removed are `default`, and no experiment override is
  // specified on the feature.
  histogram_tester_.ExpectTotalCount(kCrossOtrResponseExperimentalMetricName,
                                     0);
}

class ContextMenuIncognitoFilterComponentUpdaterExperimentBrowserTest
    : public ContextMenuIncognitoFilterComponentUpdaterBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    // Set up the feature to use an experiment override.
    std::string dummy_experiment = "mattwashere";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", "true"},
         {"experiment_identifier", dummy_experiment}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Enable open in incognito param filtering. These are default
    // classifications that should not be applied due to our use of an
    // experiment override.
    FilterClassifications classifications = MakeClassificationsProtoFromMap(
        {{"foo.com", {"plzblock1"}}, {"127.0.0.1", {"plzblockredirect"}}},
        {{"127.0.0.1", {"plzblock"}}});
    // Because we use `dummy_experiment`, we should only filter this param; the
    // rules above are ignored.
    AddClassification(classifications.add_classifications(), "foo.com",
                      FilterClassification_SiteRole_SOURCE,
                      FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                      {"plzblockexperiment"}, {}, {dummy_experiment});

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(component_dir_.GetPath(),
                                 classifications.SerializeAsString());
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected.
IN_PROC_BROWSER_TEST_F(
    ContextMenuIncognitoFilterComponentUpdaterExperimentBrowserTest,
    OpenIncognitoUrlParamFilter) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(
      embedded_test_server()->GetURL("/empty.html?plzblock=1&nochanges=2&"
                                     "plzblock1=2&plzblockexperiment=asdf"));

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the filtered URL. Because we use an experiment tag,
  // the default blocking of `plzblock` and `plzblock1` do not occur.
  GURL expected(embedded_test_server()->GetURL(
      "/empty.html?plzblock=1&nochanges=2&plzblock1=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 200 (params filtered => metrics collected), and the
  // navigation went from normal --> OTR browsing. Because an experimental param
  // was removed, we also validate that the segmented metric was written.
  histogram_tester_.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester_.ExpectUniqueSample(
      kCrossOtrResponseExperimentalMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester_.ExpectTotalCount(kApplicableSourceClassificationCount, 1);
  histogram_tester_.ExpectTotalCount(kApplicableDestinationClassificationCount,
                                     1);
  // Although additional classifications are passed, they are not applicable
  // given the experiment override.
  EXPECT_EQ(histogram_tester_.GetTotalSum(kApplicableSourceClassificationCount),
            1);
  EXPECT_EQ(
      histogram_tester_.GetTotalSum(kApplicableDestinationClassificationCount),
      0);
}

class ContextMenuIncognitoFilterComponentUpdaterAdditiveExperimentBrowserTest
    : public ContextMenuIncognitoFilterComponentUpdaterBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    // Set up the feature to use an experiment override.
    std::string dummy_experiment = "mattwashere";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", "true"},
         {"experiment_identifier", dummy_experiment}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Enable open in incognito param filtering. This verifies that
    // classifications with both the experiment and the default are applied as
    // expected.
    FilterClassifications classifications;
    AddClassification(classifications.add_classifications(), "foo.com",
                      FilterClassification_SiteRole_SOURCE,
                      FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                      {"plzblock1"}, {}, {kDefaultTag, dummy_experiment});
    AddClassification(classifications.add_classifications(), "127.0.0.1",
                      FilterClassification_SiteRole_DESTINATION,
                      FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                      {"plzblock", "plzblockredirect"}, {},
                      {kDefaultTag, dummy_experiment});
    AddClassification(classifications.add_classifications(), "foo.com",
                      FilterClassification_SiteRole_SOURCE,
                      FilterClassification_SiteMatchType_EXACT_ETLD_PLUS_ONE,
                      {"plzblockexperiment"}, {}, {dummy_experiment});

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(component_dir_.GetPath(),
                                 classifications.SerializeAsString());
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

// Enable "Open Link in Incognito Window" URL parameter filtering, and ensure
// that it filters as expected.
IN_PROC_BROWSER_TEST_F(
    ContextMenuIncognitoFilterComponentUpdaterAdditiveExperimentBrowserTest,
    OpenIncognitoUrlParamFilter) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(
      embedded_test_server()->GetURL("/empty.html?plzblock=1&nochanges=2&"
                                     "plzblock1=2&plzblockexperiment=asdf"));

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the filtered URL. Because we use an experiment tag,
  // `plzblockexperiment` is removed in addition to the default blocking of
  // `plzblock` and `plzblock1`.
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 200 (params filtered => metrics collected), and the
  // navigation went from normal --> OTR browsing. Because a param included in
  // the experiment was removed, we should also see that metric written.
  histogram_tester_.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester_.ExpectUniqueSample(
      kCrossOtrResponseExperimentalMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester_.ExpectTotalCount(kApplicableSourceClassificationCount, 1);
  histogram_tester_.ExpectTotalCount(kApplicableDestinationClassificationCount,
                                     1);
  EXPECT_EQ(histogram_tester_.GetTotalSum(kApplicableSourceClassificationCount),
            2);
  EXPECT_EQ(
      histogram_tester_.GetTotalSum(kApplicableDestinationClassificationCount),
      1);
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuIncognitoFilterComponentUpdaterAdditiveExperimentBrowserTest,
    OpenIncognitoUrlParamFilterNoExperimentalParamFiltered) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_root(
      embedded_test_server()->GetURL("/empty.html?plzblock=1&nochanges=2&"
                                     "plzblock1=2"));

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
  TestRenderViewContextMenu menu(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD, 0);

  content::WebContents* tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  // Verify that it loaded the filtered URL.
  GURL expected(embedded_test_server()->GetURL("/empty.html?nochanges=2"));
  ASSERT_EQ(expected, tab->GetLastCommittedURL());

  // The response was a 200 (params filtered => metrics collected), and the
  // navigation went from normal --> OTR browsing. Because no non-default param
  // was removed, the experimental metric should not be written.
  histogram_tester_.ExpectUniqueSample(
      kCrossOtrResponseMetricName,
      net::HttpUtil::MapStatusCodeForHistogram(200), 1);
  histogram_tester_.ExpectTotalCount(kCrossOtrResponseExperimentalMetricName,
                                     0);
}

}  // namespace url_param_filter
