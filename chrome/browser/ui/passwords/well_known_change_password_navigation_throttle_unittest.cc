// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/passwords/well_known_change_password_navigation_throttle.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// An option struct to simplify setting up a specific navigation throttle.
struct NavigationThrottleOptions {
  GURL url;
  raw_ptr<content::RenderFrameHost> rfh = nullptr;
  ui::PageTransition page_transition = ui::PAGE_TRANSITION_FROM_API;
  std::optional<url::Origin> initiator_origin;
};

}  // namespace

class WellKnownChangePasswordNavigationThrottleTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::RenderFrameHostTester::For(main_rfh())
        ->InitializeRenderFrameIfNeeded();
    subframe_ = content::RenderFrameHostTester::For(main_rfh())
                    ->AppendChild("subframe");
  }

  content::RenderFrameHost* subframe() const { return subframe_; }

  std::unique_ptr<WellKnownChangePasswordNavigationThrottle>
  CreateNavigationThrottle(NavigationThrottleOptions opts) {
    content::MockNavigationHandle handle(
        opts.url, opts.rfh ? opts.rfh.get() : main_rfh());
    handle.set_page_transition(opts.page_transition);
    if (opts.initiator_origin) {
      handle.set_initiator_origin(*opts.initiator_origin);
    }
    return WellKnownChangePasswordNavigationThrottle::MaybeCreateThrottleFor(
        &handle);
  }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> subframe_ = nullptr;
};

TEST_F(WellKnownChangePasswordNavigationThrottleTest,
       CreateNavigationThrottle_ChangePasswordUrl) {
  // change-password url without trailing slash
  GURL url("https://google.com/.well-known/change-password");
  EXPECT_TRUE(CreateNavigationThrottle({url}));

  // change-password url with trailing slash
  url = GURL("https://google.com/.well-known/change-password/");
  EXPECT_TRUE(CreateNavigationThrottle({url}));
}

TEST_F(WellKnownChangePasswordNavigationThrottleTest,
       CreateNavigationThrottle_ChangePasswordUrl_FromGoogleLink) {
  EXPECT_TRUE(CreateNavigationThrottle({
      .url = GURL("https://google.com/.well-known/change-password"),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin =
          url::Origin::Create(GURL("chrome://settings/passwords/check")),
  }));

  EXPECT_TRUE(CreateNavigationThrottle({
      .url = GURL("https://google.com/.well-known/change-password/"),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin =
          url::Origin::Create(GURL("https://passwords.google.com/checkup")),
  }));
}

TEST_F(WellKnownChangePasswordNavigationThrottleTest,
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

TEST_F(WellKnownChangePasswordNavigationThrottleTest,
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
TEST_F(WellKnownChangePasswordNavigationThrottleTest,
       NeverCreateNavigationThrottle_Subframe) {
  // change-password url without trailing slash
  GURL url("https://google.com/.well-known/change-password");
  EXPECT_FALSE(CreateNavigationThrottle({url, subframe()}));

  // change-password url with trailing slash
  url = GURL("https://google.com/.well-known/change-password/");
  EXPECT_FALSE(CreateNavigationThrottle({url, subframe()}));
}

class WellKnownChangePasswordNavigationThrottleFencedFramesTest
    : public WellKnownChangePasswordNavigationThrottleTest {
 public:
  WellKnownChangePasswordNavigationThrottleFencedFramesTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~WellKnownChangePasswordNavigationThrottleFencedFramesTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A WellKnownChangePasswordNavigationThrottle should never be created for a
// navigation initiated by a fenced frame.
TEST_F(WellKnownChangePasswordNavigationThrottleFencedFramesTest,
       NeverCreateNavigationThrottle_FencedFrame) {
  content::RenderFrameHost* fenced_frame =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();

  GURL url("https://google.com/.well-known/change-password");
  EXPECT_FALSE(CreateNavigationThrottle({url, fenced_frame}));
}
