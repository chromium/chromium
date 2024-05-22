// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/passwords/password_manager_navigation_throttle.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
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

class PasswordManagerNavigationThrottleTest
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

  std::unique_ptr<PasswordManagerNavigationThrottle> CreateNavigationThrottle(
      NavigationThrottleOptions opts) {
    content::MockNavigationHandle handle(
        opts.url, opts.rfh ? opts.rfh.get() : main_rfh());
    handle.set_page_transition(opts.page_transition);
    if (opts.initiator_origin) {
      handle.set_initiator_origin(*opts.initiator_origin);
    }
    return PasswordManagerNavigationThrottle::MaybeCreateThrottleFor(&handle);
  }

 private:
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  raw_ptr<content::RenderFrameHost, DanglingUntriaged> subframe_ = nullptr;
};

TEST_F(PasswordManagerNavigationThrottleTest,
       CreatesNavigationThrottle_HelpSite) {
  EXPECT_TRUE(CreateNavigationThrottle({
      .url = GURL(password_manager::kManageMyPasswordsURL),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin =
          url::Origin::Create(GURL(password_manager::kReferrerURL)),
  }));
}

TEST_F(PasswordManagerNavigationThrottleTest, CreatesNavigationThrottle_PGC) {
  EXPECT_TRUE(CreateNavigationThrottle({
      .url = GURL(password_manager::kManageMyPasswordsURL),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin =
          url::Origin::Create(GURL(password_manager::kManageMyPasswordsURL)),
  }));
}

TEST_F(PasswordManagerNavigationThrottleTest,
       DoesntCreateNavigationThrottleWhenOriginDoesntMatch) {
  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL(password_manager::kManageMyPasswordsURL),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin = url::Origin::Create(GURL("https://example.com/")),
  }));
}

TEST_F(PasswordManagerNavigationThrottleTest,
       DoesntCreateNavigationThrottleWhenURLDoesntMatch_HelpSite) {
  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL("https://passwords.google.com/help"),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin =
          url::Origin::Create(GURL(password_manager::kReferrerURL)),
  }));
}

TEST_F(PasswordManagerNavigationThrottleTest,
       DoesntCreateNavigationThrottleWhenURLDoesntMatch_PGC) {
  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL("https://passwords.google.com/help"),
      .page_transition = ui::PAGE_TRANSITION_LINK,
      .initiator_origin =
          url::Origin::Create(GURL(password_manager::kManageMyPasswordsURL)),
  }));
}

TEST_F(PasswordManagerNavigationThrottleTest,
       DoesntCreateNavigationThrottleWhenNotLinkTransition_HelpSite) {
  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL(password_manager::kManageMyPasswordsURL),
      .page_transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      .initiator_origin =
          url::Origin::Create(GURL(password_manager::kReferrerURL)),
  }));
}

TEST_F(PasswordManagerNavigationThrottleTest,
       DoesntCreateNavigationThrottleWhenNotLinkTransition_PGC) {
  EXPECT_FALSE(CreateNavigationThrottle({
      .url = GURL(password_manager::kManageMyPasswordsURL),
      .page_transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK,
      .initiator_origin =
          url::Origin::Create(GURL(password_manager::kManageMyPasswordsURL)),
  }));
}
