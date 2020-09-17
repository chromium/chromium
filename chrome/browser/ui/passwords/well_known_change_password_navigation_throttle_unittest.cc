// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Test with parameter for kWellKnownChangePassword feature state.
class WellKnownChangePasswordNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  WellKnownChangePasswordNavigationThrottleTest() {
    bool flag_enabled = GetParam();
    scoped_features_.InitWithFeatureState(
        password_manager::features::kWellKnownChangePassword, flag_enabled);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");
  }

  ~WellKnownChangePasswordNavigationThrottleTest() override = default;

  std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
  CreateNavigationThrottleForUrl(const GURL& url) {
    content::MockNavigationHandle handle(url, main_rfh());
    return WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(
        &handle);
  }

  std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
  CreateNavigationThrottleForUrlAndSubframe(const GURL& url) {
    content::MockNavigationHandle handle(url, subframe_);
    return WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(
        &handle);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
  content::RenderFrameHost* subframe_ = nullptr;
};

TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       CreateNavigationThrottle_ChangePasswordUrl) {
  bool flag_enabled = GetParam();
  // change-password url without trailing slash
  GURL url("https://google.com/.well-known/change-password");
  EXPECT_EQ(!!CreateNavigationThrottleForUrl(url), flag_enabled);

  // change-password url with trailing slash
  url = GURL("https://google.com/.well-known/change-password/");
  EXPECT_EQ(!!CreateNavigationThrottleForUrl(url), flag_enabled);
}

TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       NeverCreateNavigationThrottle_DifferentUrl) {
  GURL url("https://google.com/.well-known/time");
  EXPECT_FALSE(CreateNavigationThrottleForUrl(url));

  url = GURL("https://google.com/foo");
  EXPECT_FALSE(CreateNavigationThrottleForUrl(url));

  url = GURL("chrome://settings/");
  EXPECT_FALSE(CreateNavigationThrottleForUrl(url));

  url = GURL("mailto:?subject=test");
  EXPECT_FALSE(CreateNavigationThrottleForUrl(url));
}

// A WellKnownChangePasswordNavigationThrottle should never be created for a
// navigation initiated by a subframe.
TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       NeverCreateNavigationThrottle_Subframe) {
  // change-password url without trailing slash
  GURL url("https://google.com/.well-known/change-password");
  EXPECT_EQ(CreateNavigationThrottleForUrlAndSubframe(url), nullptr);

  // change-password url with trailing slash
  url = GURL("https://google.com/.well-known/change-password/");
  EXPECT_EQ(CreateNavigationThrottleForUrlAndSubframe(url), nullptr);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WellKnownChangePasswordNavigationThrottleTest,
                         testing::Bool());
