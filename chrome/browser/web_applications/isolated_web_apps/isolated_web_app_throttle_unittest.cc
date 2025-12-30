// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"

#include "base/run_loop.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

class MockIsolatedWebAppThrottle : public IsolatedWebAppThrottle {
 public:
  explicit MockIsolatedWebAppThrottle(
      content::NavigationThrottleRegistry& registry)
      : IsolatedWebAppThrottle(registry) {}
  ~MockIsolatedWebAppThrottle() override = default;

  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(void, OnComponentsReady, (), (override));
  MOCK_METHOD(void,
              CancelDeferredNavigation,
              (content::NavigationThrottle::ThrottleCheckResult),
              (override));
};

}  // namespace

constexpr char kIsolatedAppOrigin[] =
    "isolated-app://amfcf7c4bmpbjbmq4h4yptcobves56hfdyr7tm3doxqvfmsk5ss6maacai";

class IsolatedWebAppThrottleTest : public WebAppTest {
 public:
  content::WebContentsTester* web_contents_tester() const {
    return content::WebContentsTester::For(web_contents());
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  IwaPermissionsPolicyCache* GetCache() {
    return IwaPermissionsPolicyCacheFactory::GetForProfile(profile());
  }

 private:
#if !BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebApps};
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

TEST_F(IsolatedWebAppThrottleTest, NoIwaNavigationProceed) {
  content::MockNavigationHandle test_handle(
      GURL("https://notanisolatedwebapp.com"), main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(IsolatedWebAppThrottleTest, WebAppProviderNotInitialized) {
  content::MockNavigationHandle test_handle(GURL(kIsolatedAppOrigin),
                                            main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());
}

TEST_F(IsolatedWebAppThrottleTest, WebAppProviderInitialized) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  content::MockNavigationHandle test_handle(GURL(kIsolatedAppOrigin),
                                            main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);

  // If manifest fetch is needed, it should DEFER.
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());

  // If manifest is already cached, it should PROCEED.
  GetCache()->SetPolicy(IwaOrigin::Create(GURL(kIsolatedAppOrigin)).value(),
                        {});
  auto throttle2 = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle2->WillStartRequest().action());
}

TEST_F(IsolatedWebAppThrottleTest, WebAppProviderInitializedAfterNavigation) {
  content::MockNavigationHandle test_handle(GURL(kIsolatedAppOrigin),
                                            main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());

  base::RunLoop run_loop;
  EXPECT_CALL(*throttle, OnComponentsReady()).WillOnce([&run_loop]() {
    run_loop.Quit();
  });
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  run_loop.Run();
}

}  // namespace web_app
