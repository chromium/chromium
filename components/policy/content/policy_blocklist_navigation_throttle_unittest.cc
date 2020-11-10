// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_navigation_throttle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/content/policy_blocklist_service.h"
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
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using SafeSitesFilterBehavior = policy::SafeSitesFilterBehavior;

constexpr size_t kCacheSize = 2;

}  // namespace

class PolicyBlocklistNavigationThrottleTest
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  PolicyBlocklistNavigationThrottleTest() = default;
  ~PolicyBlocklistNavigationThrottleTest() override = default;

  // content::RenderViewHostTestHarness:
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    user_prefs::UserPrefs::Set(browser_context(), &pref_service_);
    policy::URLBlocklistManager::RegisterProfilePrefs(pref_service_.registry());

    // Prevent crashes in BrowserContextDependencyManager caused when tests
    // that run in serial happen to reuse a memory address for a BrowserContext
    // from a previously-run test.
    // TODO(michaelpg): This shouldn't be the test's responsibility. Can we make
    // BrowserContext just do this always, like Profile does?
    BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(
        browser_context());

    PolicyBlocklistFactory::GetInstance()
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

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto throttle = std::make_unique<PolicyBlocklistNavigationThrottle>(
        navigation_handle, browser_context());

    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }

 protected:
  std::unique_ptr<content::NavigationSimulator> StartNavigation(
      const GURL& first_url) {
    auto navigation_simulator =
        content::NavigationSimulator::CreateRendererInitiated(first_url,
                                                              main_rfh());
    navigation_simulator->SetAutoAdvance(false);
    navigation_simulator->Start();
    return navigation_simulator;
  }

  void SetBlocklistUrlPattern(const std::string& pattern) {
    auto value = std::make_unique<base::Value>(base::Value::Type::LIST);
    value->Append(base::Value(pattern));
    pref_service_.SetManagedPref(policy::policy_prefs::kUrlBlacklist,
                                 std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetAllowlistUrlPattern(const std::string& pattern) {
    auto value = std::make_unique<base::Value>(base::Value::Type::LIST);
    value->Append(base::Value(pattern));
    pref_service_.SetManagedPref(policy::policy_prefs::kUrlWhitelist,
                                 std::move(value));
    task_environment()->RunUntilIdle();
  }

  void SetSafeSitesFilterBehavior(SafeSitesFilterBehavior filter_behavior) {
    auto value =
        std::make_unique<base::Value>(static_cast<int>(filter_behavior));
    pref_service_.SetManagedPref(policy::policy_prefs::kSafeSitesFilterBehavior,
                                 std::move(value));
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  safe_search_api::StubURLChecker stub_url_checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyBlocklistNavigationThrottleTest);
};

class PolicyBlocklistNavigationThrottleWithSafeSitesErrorContentTest
    : public PolicyBlocklistNavigationThrottleTest {
 public:
  static const char kErrorPageContent[];
  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    auto throttle = std::make_unique<PolicyBlocklistNavigationThrottle>(
        navigation_handle, browser_context(), kErrorPageContent);

    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }
};

const char PolicyBlocklistNavigationThrottleWithSafeSitesErrorContentTest::
    kErrorPageContent[] = "<html><body>URL was filtered.</body></html>";

TEST_F(PolicyBlocklistNavigationThrottleTest, Blocklist) {
  SetBlocklistUrlPattern("example.com");

  // Block a blocklisted site.
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::BLOCK_REQUEST,
            navigation_simulator->GetLastThrottleCheckResult());
}

TEST_F(PolicyBlocklistNavigationThrottleTest, Allowlist) {
  SetAllowlistUrlPattern("www.example.com");
  SetBlocklistUrlPattern("example.com");

  // Allow a allowlisted exception to a blocklisted domain.
  auto navigation_simulator = StartNavigation(GURL("http://www.example.com/"));
  ASSERT_FALSE(navigation_simulator->IsDeferred());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Safe) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpValidResponse(false /* is_porn */);

  // Defer, then allow a safe site.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  EXPECT_TRUE(navigation_simulator->IsDeferred());
  navigation_simulator->Wait();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_Porn) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpValidResponse(true /* is_porn */);

  // Defer, then cancel a porn site.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  EXPECT_TRUE(navigation_simulator->IsDeferred());
  navigation_simulator->Wait();
  EXPECT_EQ(content::NavigationThrottle::CANCEL,
            navigation_simulator->GetLastThrottleCheckResult());
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

TEST_F(PolicyBlocklistNavigationThrottleTest, DISABLED_SafeSites_Failure) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);
  stub_url_checker_.SetUpFailedResponse();

  // If the Safe Search API request fails, the navigation is allowed.
  auto navigation_simulator = StartNavigation(GURL("http://example.com/"));
  EXPECT_TRUE(navigation_simulator->IsDeferred());
  navigation_simulator->Wait();
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            navigation_simulator->GetLastThrottleCheckResult());
}

TEST_F(PolicyBlocklistNavigationThrottleTest, SafeSites_CachedSites) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

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
    EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content());
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
    EXPECT_FALSE(navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content());
  }
}

TEST_F(PolicyBlocklistNavigationThrottleWithSafeSitesErrorContentTest,
       SafeSites_CachedSites) {
  SetSafeSitesFilterBehavior(SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

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
    EXPECT_STREQ(kErrorPageContent,
                 navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content()
                     ->c_str());
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
    EXPECT_STREQ(kErrorPageContent,
                 navigation_simulator->GetLastThrottleCheckResult()
                     .error_page_content()
                     ->c_str());
  }
}
