// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// An option struct to simplify setting up a specific navigation throttle.
struct NavigationThrottleOptions {
  GURL url;
  content::RenderFrameHost* rfh = nullptr;
  ui::PageTransition page_transition = ui::PAGE_TRANSITION_FROM_API;
  base::Optional<url::Origin> initiator_origin;
};

}  // namespace

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

  content::RenderFrameHost* subframe() const { return subframe_; }

  std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
  CreateNavigationThrottle(NavigationThrottleOptions opts) {
    content::MockNavigationHandle handle(opts.url,
                                         opts.rfh ? opts.rfh : main_rfh());
    handle.set_page_transition(opts.page_transition);
    if (opts.initiator_origin)
      handle.set_initiator_origin(*opts.initiator_origin);
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
  EXPECT_EQ(!!CreateNavigationThrottle({url}), flag_enabled);

  // change-password url with trailing slash
  url = GURL("https://google.com/.well-known/change-password/");
  EXPECT_EQ(!!CreateNavigationThrottle({url}), flag_enabled);
}

TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       CreateNavigationThrottle_ChangePasswordUrl_FromGoogleLink) {
  bool flag_enabled = GetParam();
  EXPECT_EQ(!!CreateNavigationThrottle({
                .url = GURL("https://google.com/.well-known/change-password"),
                .page_transition = ui::PAGE_TRANSITION_LINK,
                .initiator_origin = url::Origin::Create(
                    GURL("chrome://settings/passwords/check")),
            }),
            flag_enabled);

  EXPECT_EQ(!!CreateNavigationThrottle({
                .url = GURL("https://google.com/.well-known/change-password/"),
                .page_transition = ui::PAGE_TRANSITION_LINK,
                .initiator_origin = url::Origin::Create(
                    GURL("https://passwords.google.com/checkup")),
            }),
            flag_enabled);
}

TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       NeverCreateNavigationThrottle_FromOtherLink) {
  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL("https://google.com/.well-known/change-password"),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin = url::Origin::Create(GURL("https://example.com")),
  }));

  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL("https://google.com/.well-known/change-password/"),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin = url::Origin::Create(GURL("https://example.com")),
  }));
}

TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       NeverCreateNavigationThrottle_DifferentUrl) {
  GURL url("https://google.com/.well-known/time");
  EXPECT_FALSE(CreateNavigationThrottle({url}));

  url = GURL("https://google.com/foo");
  EXPECT_FALSE(CreateNavigationThrottle({url}));

  url = GURL("chrome://settings/");
  EXPECT_FALSE(CreateNavigationThrottle({url}));

  url = GURL("mailto:?subject=test");
  EXPECT_FALSE(CreateNavigationThrottle({url}));
}

// A WellKnownChangePasswordNavigationThrottle should never be created for a
// navigation initiated by a subframe.
TEST_P(WellKnownChangePasswordNavigationThrottleTest,
       NeverCreateNavigationThrottle_Subframe) {
  // change-password url without trailing slash
  GURL url("https://google.com/.well-known/change-password");
  EXPECT_EQ(CreateNavigationThrottle({url, subframe()}), nullptr);

  // change-password url with trailing slash
  url = GURL("https://google.com/.well-known/change-password/");
  EXPECT_EQ(CreateNavigationThrottle({url, subframe()}), nullptr);
}

INSTANTIATE_TEST_SUITE_P(All,
                         WellKnownChangePasswordNavigationThrottleTest,
                         testing::Bool());
