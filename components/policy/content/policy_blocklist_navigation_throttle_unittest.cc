// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/policy/content/safe_search_service.h"
#include "components/policy/content/safe_sites_navigation_throttle.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/browser/url_blocklist_policy_handler.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/safe_search_api/stub_url_checker.h"
#include "components/safe_search_api/url_checker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using SafeSitesFilterBehavior = policy::SafeSitesFilterBehavior;

constexpr size_t kCacheSize = 2;

}  // namespace

// TODO(crbug.com/40156526): Break out the tests into separate files. The
// SafeSites tests should be parameterized to run the same tests on both types.
class SafeSitesNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  SafeSitesNavigationThrottleTest() = default;
  SafeSitesNavigationThrottleTest(const SafeSitesNavigationThrottleTest&) =
      delete;
  SafeSitesNavigationThrottleTest& operator=(
      const SafeSitesNavigationThrottleTest&) = delete;
  ~SafeSitesNavigationThrottleTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Prevent crashes in BrowserContextDependencyManager caused when tests
    // that run in serial happen to reuse a memory address for a BrowserContext
    // from a previously-run test.
    // TODO(michaelpg): This shouldn't be the test's responsibility. Can we make
    // BrowserContext just do this always, like Profile does?
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        browser_context());

    SafeSearchFactory::GetInstance()
        ->GetForBrowserContext(browser_context())
        ->SetSafeSearchURLCheckerForTest(
            stub_url_checker_.BuildURLChecker(kCacheSize));

    // Observe the WebContents to add the throttle.
    Observe(RenderViewHostTestHarness::web_contents());
  }

  void TearDown() override {
    BrowserContextDependencyManager::GetInstance()
        ->DestroyBrowserContextServices(browser_context());
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto throttle = std::make_unique<SafeSitesNavigationThrottle>(
        navigation_handle, browser_context());

    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }

  std::unique_ptr<content::NavigationSimulator> StartNavigation(
      const GURL& first_url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
    navigation_simulator->SetAutoAdvance(false);
    navigation_simulator->Start();
    return navigation_simulator;
  }

  // Tests that redirects from a safe site to a porn site are handled correctly.
  // Also tests the same scenario when the sites are in the cache.
  // If |expected_error_page_content| is not null, the canceled throttle check
  // result's error_page_content will be expected to match it.
  void TestSafeSitesRedirectAndCachedSites(
      const char* expected_error_page_content);

  // Tests responses for both a safe site and a porn site both when the sites
  // are in the cache and not. If |expected_error_page_content| is not null, the
  // canceled throttle check result's error_page_content will be expected to
  // match it.
  void TestSafeSitesCachedSites(const char* expected_error_page_content);

  safe_search_api::StubURLChecker stub_url_checker_;
};

class SafeSitesNavigationThrottleWithErrorContentTest
    : public SafeSitesNavigationThrottleTest {
 protected:
  static const char kErrorPageContent[];

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto throttle = std::make_unique<SafeSitesNavigationThrottle>(
        navigation_handle, browser_context(), kErrorPageContent);

    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }
};

const char
    SafeSitesNavigationThrottleWithErrorContentTest::kErrorPageContent[] =
        "<html><body>URL was filtered.</body></html>";

class PolicyBlocklistNavigationThrottleTest
    : public SafeSitesNavigationThrottleTest {
 public:
  void SetUp() override {
    SafeSitesNavigationThrottleTest::SetUp();

    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
    policy::URLBlocklistManager::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto throttle = std::make_unique<PolicyBlocklistNavigationThrottle>(
        navigation_handle, browser_context());

    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }

  void SetBlocklistUrlPattern(const std::string& pattern) {
    base::Value::List value;
    value.Append(pattern);
    pref_service_.SetManagedPref(policy::policy_prefs::kUrlBlocklist,
                                 std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetAllowlistUrlPattern(const std::string& pattern) {
    base::Value::List value;
    value.Append(pattern);
    pref_service_.SetManagedPref(policy::policy_prefs::kUrlAllowlist,
                                 std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetSafeSitesFilterBehavior(SafeSitesFilterBehavior filter_behavior) {
    base::Value value(static_cast<int>(filter_behavior));
    pref_service_.SetManagedPref(policy::policy_prefs::kSafeSitesFilterBehavior,
                                 std::move(value));
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(PolicyBlocklistNavigationThrottleTest, Blocklist) {
  base::HistogramTester histogram_tester;

  SetBlocklistUrlPattern("example.com");

  // Block a blocklisted site.
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST,
            navigation_simulator->GetLastThrottleCheckResult());

  // Call WebContents::Stop() to reset the main rfh's navigation state. It
  // results in destructing the navigation throttles to flush metrics.
  RenderViewHostTestHarness::web_contents()->Stop();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Throttles.PolicyBlocklist.RequestThrottleAction2",
      PolicyBlocklistNavigationThrottle::RequestThrottleAction::kBlock, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Navigation.Throttles.PolicyBlocklist.DeferDurationTime",
      base::TimeDelta(), 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Throttles.PolicyBlocklist.RequestToResponseTime", 0);
}

TEST_F(PolicyBlocklistNavigationThrottleTest, Allowlist) {
  base::HistogramTester histogram_tester;

  SetAllowlistUrlPattern("www.example.com");
  SetBlocklistUrlPattern("example.com");

  // Allow a allowlisted exception to a blocklisted domain.
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());

  // Call WebContents::Stop() to reset the main rfh's navigation state. It
  // results in destructing the navigation throttles to flush metrics.
  RenderViewHostTestHarness::web_contents()->Stop();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Throttles.PolicyBlocklist.RequestThrottleAction2",
      PolicyBlocklistNavigationThrottle::RequestThrottleAction::kProceed, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Navigation.Throttles.PolicyBlocklist.DeferDurationTime",
      base::TimeDelta(), 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Throttles.PolicyBlocklist.RequestToResponseTime", 0);
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Safe) {
  base::HistogramTester histogram_tester;

  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpValidResponse(false /* is_porn */);

  // Defer, then allow a safe site.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  EXPECT_TRUE(navigation_simulator->IsDeferred());
  navigation_simulator->Wait();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());

  // Call WebContents::Stop() to reset the main rfh's navigation state. It
  // results in destructing the navigation throttles to flush metrics.
  RenderViewHostTestHarness::web_contents()->Stop();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Throttles.PolicyBlocklist.RequestThrottleAction2",
      PolicyBlocklistNavigationThrottle::RequestThrottleAction::kDefer, 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Throttles.PolicyBlocklist.DeferDurationTime", 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Throttles.PolicyBlocklist.RequestToResponseTime", 0);
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Porn) {
  base::HistogramTester histogram_tester;

  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpValidResponse(true /* is_porn */);

  // Defer, then cancel a porn site.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  EXPECT_TRUE(navigation_simulator->IsDeferred());
  navigation_simulator->Wait();
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());

  // Call WebContents::Stop() to reset the main rfh's navigation state. It
  // results in destructing the navigation throttles to flush metrics.
  RenderViewHostTestHarness::web_contents()->Stop();

  histogram_tester.ExpectUniqueSample(
      "Navigation.Throttles.PolicyBlocklist.RequestThrottleAction2",
      PolicyBlocklistNavigationThrottle::RequestThrottleAction::kDefer, 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Throttles.PolicyBlocklist.DeferDurationTime", 1);
  histogram_tester.ExpectTotalCount(
      "Navigation.Throttles.PolicyBlocklist.RequestToResponseTime", 0);
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Allowlisted) {
  SetAllowlistUrlPattern("example.com");
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpValidResponse(true /* is_porn */);

  // Even with SafeSites enabled, a allowlisted site is immediately allowed.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Schemes) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpValidResponse(true /* is_porn */);

  // The safe sites filter is only used for http(s) URLs. This test uses
  // browser-initiated navigation, since renderer-initiated navigations to
  // WebUI documents are not allowed.
  auto navigation_simulator =
      content::NavigationSimulator::CreateBrowserInitiated(
          GURL("chrome://settings"), RenderViewHostTestHarness::web_contents());
  navigation_simulator->SetAutoAdvance(false);
  navigation_simulator->Start();
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_PolicyChange) {
  stub_url_checker_.SetUpValidResponse(true /* is_porn */);

  // The safe sites filter is initially disabled.
  {
    auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
    ASSERT_FALSE(navigation_simulator->IsDeferred());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator->GetLastThrottleCheckResult());
  }

  // Setting the pref enables the filter.
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  {
    auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
    EXPECT_TRUE(navigation_simulator->IsDeferred());
    navigation_simulator->Wait();
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              navigation_simulator->GetLastThrottleCheckResult());
  }

  // Updating the pref disables the filter.
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterDisabled);
  {
    auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
    ASSERT_FALSE(navigation_simulator->IsDeferred());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator->GetLastThrottleCheckResult());
  }
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Failure) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpFailedResponse();

  // If the Safe Search API request fails, the navigation is allowed.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  EXPECT_TRUE(navigation_simulator->IsDeferred());
  navigation_simulator->Wait();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}

void SafeSitesNavigationThrottleTest::TestSafeSitesCachedSites(
    const char* expected_error_page_content) {
  // Check a couple of sites.
  ASSERT_EQ(2u, kCacheSize);
  const GURL safe_site = GURL("http://example.com/");
  const GURL porn_site = GURL("http://example2.com/");

  stub_url_checker_.SetUpValidResponse(false /* is_porn */);
  {
    auto navigation_simulator = StartNavigation(safe_site);
    EXPECT_TRUE(navigation_simulator->IsDeferred());
    navigation_simulator->Wait();
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator->GetLastThrottleCheckResult());
    EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content());
  }

  stub_url_checker_.SetUpValidResponse(true /* is_porn */);
  {
    auto navigation_simulator = StartNavigation(porn_site);
    EXPECT_TRUE(navigation_simulator->IsDeferred());
    navigation_simulator->Wait();
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              navigation_simulator->GetLastThrottleCheckResult());
    if (expected_error_page_content) {
      EXPECT_STREQ(expected_error_page_content,
                   navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content()
                       ->c_str());
    } else {
      EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content());
    }
  }

  stub_url_checker_.ClearResponses();
  {
    // This check is synchronous since the site is in the cache.
    auto navigation_simulator = StartNavigation(safe_site);
    ASSERT_FALSE(navigation_simulator->IsDeferred());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator->GetLastThrottleCheckResult());
    EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content());
  }

  {
    // This check is synchronous since the site is in the cache.
    auto navigation_simulator = StartNavigation(porn_site);
    ASSERT_FALSE(navigation_simulator->IsDeferred());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              navigation_simulator->GetLastThrottleCheckResult());
    if (expected_error_page_content) {
      EXPECT_STREQ(expected_error_page_content,
                   navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content()
                       ->c_str());
    } else {
      EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content());
    }
  }
}

TEST_F(SafeSitesNavigationThrottleTest, SafeSites_CachedSites) {
  TestSafeSitesCachedSites(nullptr);
}

TEST_F(SafeSitesNavigationThrottleWithErrorContentTest, SafeSites_CachedSites) {
  TestSafeSitesCachedSites(&kErrorPageContent[0]);
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_CachedSites) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  TestSafeSitesCachedSites(nullptr);
}

void SafeSitesNavigationThrottleTest::TestSafeSitesRedirectAndCachedSites(
    const char* expected_error_page_content) {
  // Check a couple of sites.
  ASSERT_EQ(2u, kCacheSize);
  const GURL safe_site = GURL("http://example.com/");
  const GURL porn_site = GURL("http://example2.com/");

  stub_url_checker_.SetUpValidResponse(false /* is_porn */);
  {
    auto navigation_simulator = StartNavigation(safe_site);
    EXPECT_TRUE(navigation_simulator->IsDeferred());
    navigation_simulator->Wait();
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator->GetLastThrottleCheckResult());
    EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content());

    stub_url_checker_.SetUpValidResponse(true /* is_porn */);
    navigation_simulator->Redirect(porn_site);
    EXPECT_TRUE(navigation_simulator->IsDeferred());
    navigation_simulator->Wait();
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              navigation_simulator->GetLastThrottleCheckResult());
    if (expected_error_page_content) {
      EXPECT_STREQ(expected_error_page_content,
                   navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content()
                       ->c_str());
    } else {
      EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content());
    }
  }

  stub_url_checker_.ClearResponses();
  {
    // This check is synchronous since the site is in the cache.
    auto navigation_simulator = StartNavigation(safe_site);
    ASSERT_FALSE(navigation_simulator->IsDeferred());
    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              navigation_simulator->GetLastThrottleCheckResult());
    EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content());

    navigation_simulator->Redirect(porn_site);
    ASSERT_FALSE(navigation_simulator->IsDeferred());
    EXPECT_EQ(content::NavigationThrottle::CANCEL,
              navigation_simulator->GetLastThrottleCheckResult());
    if (expected_error_page_content) {
      EXPECT_STREQ(expected_error_page_content,
                   navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content()
                       ->c_str());
    } else {
      EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                       .error_page_content());
    }
  }
}

TEST_F(SafeSitesNavigationThrottleTest, SafeSites_RedirectAndCachedSites) {
  TestSafeSitesRedirectAndCachedSites(nullptr);
}

TEST_F(SafeSitesNavigationThrottleWithErrorContentTest,
       SafeSites_RedirectAndCachedSites) {
  TestSafeSitesRedirectAndCachedSites(&kErrorPageContent[0]);
}

TEST_F(PolicyBlocklistNavigationThrottleTest,
       SafeSites_RedirectAndCachedSites) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  TestSafeSitesRedirectAndCachedSites(nullptr);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PolicyBlocklistNavigationThrottleTest, UseVpnPreConnectFiltering) {
  SetBlocklistUrlPattern("block-by-general-pref.com");
  base::Value::List list;
  list.Append("allowed-preconnect.com");
  pref_service_.SetManagedPref(
      policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist,
      base::Value(std::move(list)));

  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser_context());
  service->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      /*enforced=*/true);

  task_environment()->RunUntilIdle();

  // Verify that the pref kAlwaysOnVpnPreConnectUrlAllowlist is enforced
  // while kEnforceAlwaysOnVpnPreConnectUrlAllowlist is true.
  auto navigation_simulator =
      StartNavigation(GURL("http://allowed-preconnect.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
  navigation_simulator =
      StartNavigation(GURL("http://neural-by-general-pref.com/"));
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST,
            navigation_simulator->GetLastThrottleCheckResult());

  service->SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
      /*enforced=*/false);

  task_environment()->RunUntilIdle();

  navigation_simulator =
      StartNavigation(GURL("http://block-by-general-pref.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST,
            navigation_simulator->GetLastThrottleCheckResult());
  navigation_simulator =
      StartNavigation(GURL("http://neural-by-general-pref.com/"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}
#endif
