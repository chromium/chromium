// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"

#include <memory>

#include "base/test/gmock_expected_support.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace web_app {

namespace {
class MockIsolatedWebAppThrottle : public IsolatedWebAppThrottle {
 public:
  explicit MockIsolatedWebAppThrottle(
      content::NavigationThrottleRegistry& registry)
      : IsolatedWebAppThrottle(registry) {}
  ~MockIsolatedWebAppThrottle() override = default;

  MOCK_METHOD(void, Resume, (), (override));
};
}  // namespace

constexpr char isolated_app_origin[] =
    "isolated-app://amfcf7c4bmpbjbmq4h4yptcobves56hfdyr7tm3doxqvfmsk5ss6maacai";

class IsolatedWebAppThrottleTest : public WebAppTest {
 public:
#if !BUILDFLAG(IS_CHROMEOS)
  IsolatedWebAppThrottleTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kIsolatedWebApps},
        /*disabled_features=*/{});
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  content::WebContentsTester* web_contents_tester() const {
    return content::WebContentsTester::For(web_contents());
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

 private:
#if !BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // !BUILDFLAG(IS_CHROMEOS)
};

TEST_F(IsolatedWebAppThrottleTest, NoIwaNavigationProceed) {
  content::MockNavigationHandle test_handle(
      GURL("https://notanisolatedwebapp.com"), main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_CALL(*throttle, Resume()).Times(0);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(IsolatedWebAppThrottleTest, WebAppProviderNotInitialized) {
  content::MockNavigationHandle test_handle(GURL(isolated_app_origin),
                                            main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_CALL(*throttle, Resume()).Times(0);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());
}

TEST_F(IsolatedWebAppThrottleTest, WebAppProviderInitialized) {
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  content::MockNavigationHandle test_handle(GURL(isolated_app_origin),
                                            main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<IsolatedWebAppThrottle>(test_registry);
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());
}

TEST_F(IsolatedWebAppThrottleTest, WebAppProviderInitializedAfterNavigation) {
  content::MockNavigationHandle test_handle(GURL(isolated_app_origin),
                                            main_frame());
  content::MockNavigationThrottleRegistry test_registry(
      &test_handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  auto throttle = std::make_unique<MockIsolatedWebAppThrottle>(test_registry);
  EXPECT_CALL(*throttle, Resume()).Times(0);
  EXPECT_EQ(content::NavigationThrottle::DEFER,
            throttle->WillStartRequest().action());

  EXPECT_CALL(*throttle, Resume()).Times(1);
  test::AwaitStartWebAppProviderAndSubsystems(profile());
}

}  // namespace web_app
