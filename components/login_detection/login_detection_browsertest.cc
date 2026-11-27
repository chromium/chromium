// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/login_detection/login_detection_keyed_service.h"
#include "components/login_detection/login_detection_prefs.h"
#include "components/login_detection/login_detection_tab_helper.h"
#include "components/login_detection/login_detection_type.h"
#include "components/login_detection/login_detection_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace login_detection {

class LoginDetectionBrowserTest : public content::ContentBrowserTest {
 public:
  LoginDetectionBrowserTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {kLoginDetection, {}},
            {site_isolation::features::kSiteIsolationForOAuthSites, {}},
        },
        {});
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    https_test_server_.ServeFilesFromSourceDirectory("content/test/data");
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    ASSERT_TRUE(https_test_server_.Start());
    user_prefs::UserPrefs::Set(GetWebContents()->GetBrowserContext(),
                               &pref_service_);
    LoginDetectionTabHelper::MaybeCreateForWebContents(GetWebContents(),
                                                       base::NullCallback());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    content::ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
    content::ContentBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Verifies the histograms for the given login detection type to be recorded.
  void ExpectLoginDetectionTypeMetric(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
  }

  // Verifies the histograms for the given login detection type to be recorded
  // with the given expected bucket count for the prerendering test.
  void ExpectLoginDetectionTypeMetric(
      LoginDetectionType type,
      base::HistogramBase::Count32 expected_bucket_count) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          expected_bucket_count);
  }

  content::WebContents* GetWebContents() { return shell()->web_contents(); }

 protected:
  TestingPrefServiceSimple pref_service_;
  net::EmbeddedTestServer https_test_server_;
  content::ContentMockCertVerifier cert_verifier_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest, PopUpBasedOAuthLoginFlow) {
  // Navigate to the OAuth requestor.
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(),
      https_test_server_.GetURL("www.foo.com", "/title1.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // Create a popup for the navigation flow.
  const GURL popup_url = https_test_server_.GetURL(
      "www.oauthprovider.com", "/title2.html?client_id=123");
  content::TestNavigationObserver observer(popup_url);
  observer.StartWatchingNewWebContents();
  content::ShellAddedObserver new_shell_observer;
  ASSERT_TRUE(content::ExecJs(
      GetWebContents(),
      content::JsReplace(
          "window.open($1, 'oauth_window', 'width=10,height=10');",
          popup_url)));
  content::Shell* popup_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(popup_shell);
  auto* popup_contents = popup_shell->web_contents();
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(observer.last_navigation_succeeded());
  // This popup navigation is treated as not logged-in too.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // When the popup is closed, it will be detected as OAuth login.
  content::WebContentsDestroyedWatcher destroyed_watcher(popup_contents);
  EXPECT_TRUE(content::ExecJs(popup_contents, "window.close()"));
  destroyed_watcher.Wait();
  ExpectLoginDetectionTypeMetric(
      LoginDetectionType::kOauthPopUpFirstTimeLoginFlow);
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_,
                                               GURL("https://www.foo.com")));
  ASSERT_TRUE(
      site_isolation::SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled());
  EXPECT_TRUE(content::ChildProcessSecurityPolicy::GetInstance()
                  ->IsIsolatedSiteFromSource(
                      url::Origin::Create(GURL("https://foo.com")),
                      content::ChildProcessSecurityPolicy::
                          IsolatedOriginSource::USER_TRIGGERED));
}

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest,
                       SimplePageLoadRecordsNoLogin) {
  // Navigate to a simple page.
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(),
      https_test_server_.GetURL("www.foo.com", "/empty.html")));

  // Expect kNoLogin to be recorded for a standard page load.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
}

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest, OAuthLoginFlowRecordsMetric) {
  // Navigate to the OAuth requester site.
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(),
      https_test_server_.GetURL("www.foo.com", "/title1.html")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // Simulate the OAuth start navigation.
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(),
      https_test_server_.GetURL("www.oauthprovider.com",
                                "/title2.html?client_id=123")));
  // The start navigation itself doesn't complete the flow, so kNoLogin is
  // expected.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);
  ResetHistogramTester();

  // Simulate the OAuth completion navigation.
  ASSERT_TRUE(content::NavigateToURL(
      GetWebContents(), https_test_server_.GetURL(
                            "www.foo.com", "/title1.html#access_token=token")));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kOauthFirstTimeLoginFlow);
  EXPECT_TRUE(prefs::IsSiteInOAuthSignedInList(&pref_service_,
                                               GURL("https://www.foo.com")));
  ASSERT_TRUE(
      site_isolation::SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled());
  EXPECT_TRUE(content::ChildProcessSecurityPolicy::GetInstance()
                  ->IsIsolatedSiteFromSource(
                      url::Origin::Create(GURL("https://foo.com")),
                      content::ChildProcessSecurityPolicy::
                          IsolatedOriginSource::USER_TRIGGERED));
}

IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest,
                       KeyedServiceIsolatesStoredOAuthSites) {
  prefs::SaveSiteToOAuthSignedInList(&pref_service_,
                                     GURL("https://stored-oauth.com"));
  ResetHistogramTester();

  LoginDetectionKeyedService keyed_service(
      GetWebContents()->GetBrowserContext());

  ASSERT_TRUE(
      site_isolation::SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled());
  EXPECT_TRUE(content::ChildProcessSecurityPolicy::GetInstance()
                  ->IsIsolatedSiteFromSource(
                      url::Origin::Create(GURL("https://stored-oauth.com")),
                      content::ChildProcessSecurityPolicy::
                          IsolatedOriginSource::USER_TRIGGERED));
  histogram_tester_->ExpectUniqueSample("SiteIsolation.SavedOAuthSites.Size", 1,
                                        1);
}

class LoginDetectionPrerenderBrowserTest : public LoginDetectionBrowserTest {
 public:
  LoginDetectionPrerenderBrowserTest() {
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&LoginDetectionPrerenderBrowserTest::GetWebContents,
                            base::Unretained(this)));
  }
  ~LoginDetectionPrerenderBrowserTest() override = default;
  LoginDetectionPrerenderBrowserTest(
      const LoginDetectionPrerenderBrowserTest&) = delete;

  LoginDetectionPrerenderBrowserTest& operator=(
      const LoginDetectionPrerenderBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_->RegisterServerRequestMonitor(embedded_test_server());
    LoginDetectionBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    LoginDetectionBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return *prerender_helper_;
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(LoginDetectionPrerenderBrowserTest,
                       PrerenderingShouldNotRecordLoginDetectionMetrics) {
  GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  GURL prerender_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(GetWebContents(), initial_url));
  ResetHistogramTester();

  const content::PrerenderHostId host_id =
      prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  // Login detection metric should not be recorded in prerendering.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 0);

  // Activate the prerendered page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  // The page should be activated from the prerendering.
  EXPECT_TRUE(host_observer.was_activated());
  // Login detection metric should be recorded after activating.
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin, 1);
}

}  // namespace login_detection
