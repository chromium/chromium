// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_referrer_throttle.h"

#include <memory>

#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace dom_distiller {

namespace {

class DistillerReferrerThrottleTest
    : public content::RenderViewHostTestHarness {
 public:
  DistillerReferrerThrottleTest() = default;
  ~DistillerReferrerThrottleTest() override = default;

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }
};

TEST_F(DistillerReferrerThrottleTest, AddsThrottleForDistillerInitiator) {
  GURL original_url("https://test.com");
  GURL distiller_url = url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, original_url, "Title");

  content::MockNavigationHandle handle(GURL("https://google.com"), main_rfh());
  handle.set_initiator_origin(url::Origin::Create(distiller_url));
  NavigateAndCommit(distiller_url);

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  DistillerReferrerThrottle::MaybeCreateAndAdd(registry);
  EXPECT_TRUE(registry.ContainsHeldThrottle("DistillerReferrerThrottle"));
}

TEST_F(DistillerReferrerThrottleTest,
       DoesNotAddThrottleForNonDistillerInitiator) {
  content::MockNavigationHandle handle(GURL("https://google.com"), main_rfh());
  handle.set_initiator_origin(url::Origin::Create(GURL("https://example.com")));
  NavigateAndCommit(GURL("https://example.com"));

  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);
  DistillerReferrerThrottle::MaybeCreateAndAdd(registry);
  EXPECT_FALSE(registry.ContainsHeldThrottle("DistillerReferrerThrottle"));
}

TEST_F(DistillerReferrerThrottleTest, SetsReferrerForDistillerInitiator) {
  GURL original_url("https://test.com");
  GURL distiller_url = url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, original_url, "Title");

  content::MockNavigationHandle handle(GURL("https://google.com"), main_rfh());
  handle.set_initiator_origin(url::Origin::Create(distiller_url));

  // Set WebContents to distiller URL.
  NavigateAndCommit(distiller_url);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle = std::make_unique<DistillerReferrerThrottle>(registry);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());

  EXPECT_EQ(original_url, handle.GetReferrer().url);
  EXPECT_EQ(network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin,
            handle.GetReferrer().policy);
}

TEST_F(DistillerReferrerThrottleTest,
       DoesNotSetReferrerForNonDistillerInitiator) {
  GURL original_url("https://test.com");
  content::MockNavigationHandle handle(GURL("https://google.com"), main_rfh());
  handle.set_initiator_origin(url::Origin::Create(original_url));

  NavigateAndCommit(original_url);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle = std::make_unique<DistillerReferrerThrottle>(registry);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());

  EXPECT_TRUE(handle.GetReferrer().url.is_empty());
}

TEST_F(DistillerReferrerThrottleTest,
       SetsReferrerWhenInitiatorIsNullButWebContentsIsDistiller) {
  GURL original_url("https://test.com");
  GURL distiller_url = url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, original_url, "Title");

  content::MockNavigationHandle handle(GURL("https://google.com"), main_rfh());

  // WebContents is at distiller URL.
  NavigateAndCommit(distiller_url);

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle = std::make_unique<DistillerReferrerThrottle>(registry);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());

  EXPECT_EQ(original_url, handle.GetReferrer().url);
}

// Ensure stale distiller state is not used when WebContents URL does not match
// scheme.
TEST_F(DistillerReferrerThrottleTest,
       DoesNotSetReferrerWhenNavigatingAwayFromDistiller) {
  GURL original_url("https://test.com");
  GURL distiller_url = url_utils::GetDistillerViewUrlFromUrl(
      kDomDistillerScheme, original_url, "Title");

  content::MockNavigationHandle handle(GURL("https://google.com"), main_rfh());
  handle.set_initiator_origin(url::Origin::Create(distiller_url));

  // WebContents is at a different URL.
  NavigateAndCommit(GURL("https://example.com"));

  content::MockNavigationThrottleRegistry registry(&handle);
  auto throttle = std::make_unique<DistillerReferrerThrottle>(registry);

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillStartRequest().action());

  EXPECT_TRUE(handle.GetReferrer().url.is_empty());
}

}  // namespace

}  // namespace dom_distiller
