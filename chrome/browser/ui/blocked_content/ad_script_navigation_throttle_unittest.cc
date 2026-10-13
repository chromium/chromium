// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/ad_script_navigation_throttle.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/visibility.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "content/public/test/navigation_simulator.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class TestNavigationHandle : public content::MockNavigationHandle {
 public:
  TestNavigationHandle(const GURL& url,
                       content::RenderFrameHost* render_frame_host)
      : content::MockNavigationHandle(url, render_frame_host) {}

  ~TestNavigationHandle() override = default;

  content::MockNavigationThrottleRegistry& registry() { return registry_; }

  void set_started_by_ad(bool started_by_ad) { started_by_ad_ = started_by_ad; }
  bool StartedByAd() override { return started_by_ad_; }

  void set_started_with_transient_activation(bool val) {
    started_with_transient_activation_ = val;
  }
  bool StartedWithTransientActivation() override {
    return started_with_transient_activation_;
  }

  void set_is_navigating_from_initial_empty_document(bool val) {
    is_navigating_from_initial_empty_document_ = val;
  }
  bool IsNavigatingFromInitialEmptyDocument() const override {
    return is_navigating_from_initial_empty_document_;
  }

  void set_origin_to_commit(std::optional<url::Origin> origin_to_commit) {
    origin_to_commit_ = std::move(origin_to_commit);
    has_custom_origin_to_commit_ = true;
  }
  std::optional<url::Origin> GetOriginToCommit() override {
    if (has_custom_origin_to_commit_) {
      return origin_to_commit_;
    }
    return url::Origin::Create(GetURL());
  }

 private:
  bool started_by_ad_ = false;
  bool started_with_transient_activation_ = false;
  bool is_navigating_from_initial_empty_document_ = false;
  bool has_custom_origin_to_commit_ = false;
  std::optional<url::Origin> origin_to_commit_;
  content::MockNavigationThrottleRegistry registry_{this};
};

}  // namespace

class AdScriptNavigationThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  AdScriptNavigationThrottleTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto embedder_interface = std::make_unique<
        page_load_metrics::TestMetricsWebContentsObserverEmbedder>();
    embedder_interface_ = embedder_interface.get();
    auto* observer =
        page_load_metrics::MetricsWebContentsObserver::CreateForWebContents(
            web_contents(), std::move(embedder_interface));
    observer->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  void TearDown() override {
    embedder_interface_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<AdScriptNavigationThrottle> CreateThrottle(
      TestNavigationHandle* handle) {
    return std::make_unique<AdScriptNavigationThrottle>(handle->registry());
  }

  void NavigateToInitialPage() {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL("https://a.com/page"));
  }

  std::unique_ptr<TestNavigationHandle> CreateAdNavigation(
      const GURL& target_url) {
    auto handle =
        std::make_unique<TestNavigationHandle>(target_url, main_rfh());
    handle->set_is_renderer_initiated(true);
    handle->set_started_by_ad(true);
    handle->set_initiator_origin(url::Origin::Create(GURL("https://a.com")));
    return handle;
  }

 protected:
  bool HasRecordedUseCounter() const {
    for (const auto& feature : embedder_interface_->observed_features()) {
      if (feature.type() == blink::mojom::UseCounterFeatureType::kWebFeature &&
          feature.value() ==
              static_cast<uint32_t>(
                  blink::mojom::WebFeature::
                      kAdScriptMainFrameNavigationWithoutUserGesture)) {
        return true;
      }
    }
    return false;
  }

  raw_ptr<page_load_metrics::TestMetricsWebContentsObserverEmbedder>
      embedder_interface_ = nullptr;
};

TEST_F(AdScriptNavigationThrottleTest, MaybeCreateAndAdd) {
  content::MockNavigationHandle handle(GURL("https://example.com"), main_rfh());
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  AdScriptNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_TRUE(registry.ContainsHeldThrottle("AdScriptNavigationThrottle"));
}

TEST_F(AdScriptNavigationThrottleTest, MaybeCreateAndAdd_Subframe) {
  content::MockNavigationHandle handle(GURL("https://example.com"), main_rfh());
  handle.set_is_in_primary_main_frame(false);
  content::MockNavigationThrottleRegistry registry(
      &handle,
      content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

  AdScriptNavigationThrottle::MaybeCreateAndAdd(registry);
  EXPECT_FALSE(registry.ContainsHeldThrottle("AdScriptNavigationThrottle"));
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_BrowserInitiated) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  handle->set_is_renderer_initiated(false);
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_NonAdNavigation) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  handle->set_started_by_ad(false);
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_StartedWithTransientActivation) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  handle->set_started_with_transient_activation(true);
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_NoOriginToCommit) {
  NavigateToInitialPage();

  // `GetOriginToCommit()` returns `std::nullopt` for responses that do not
  // commit a document (such as downloads or HTTP 204/205 responses).
  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  handle->set_origin_to_commit(std::nullopt);
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_SameSite) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(GURL("https://sub.a.com/target"));
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_SameSiteBlobUrl) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(GURL("blob:https://a.com/some-uuid"));
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest,
       Allowed_NavigatingFromInitialEmptyDocument) {
  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  handle->set_is_navigating_from_initial_empty_document(true);
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(AdScriptNavigationThrottleTest, Allowed_ExtensionSchemeTarget) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/page.html"));
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest, Allowed_ExtensionSchemeSource) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(),
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/page.html"));

  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}
#endif

TEST_F(AdScriptNavigationThrottleTest,
       Proceeds_CrossSiteToPage_EvenIfSameSiteToInitiatorOrigin) {
  // Top-level page is a.com.
  NavigateToInitialPage();

  // Ad iframe from b.com navigates main frame to b.com/target without gesture.
  auto handle = CreateAdNavigation(GURL("https://b.com/target"));
  handle->set_initiator_origin(url::Origin::Create(GURL("https://b.com")));
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_TRUE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest,
       ServerRedirect_RecordsFeatureOnCrossSiteRedirect) {
  NavigateToInitialPage();

  auto handle = CreateAdNavigation(GURL("https://sub.a.com/start"));
  auto throttle = CreateThrottle(handle.get());

  // Initially same-site to a.com: not checked on start.
  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  EXPECT_FALSE(HasRecordedUseCounter());

  // Cross-site redirect: not checked on redirect, but flagged when processing
  // the final cross-site response.
  handle->set_url(GURL("https://b.com/redirected"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  EXPECT_FALSE(HasRecordedUseCounter());

  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_TRUE(HasRecordedUseCounter());
}

TEST_F(AdScriptNavigationThrottleTest,
       Allowed_CrossSiteBounceRedirectBackToSameSite) {
  NavigateToInitialPage();

  // Starts cross-site to b.com.
  auto handle = CreateAdNavigation(GURL("https://b.com/bounce"));
  auto throttle = CreateThrottle(handle.get());

  EXPECT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
  EXPECT_FALSE(HasRecordedUseCounter());

  // Redirects back to a.com before response is processed.
  handle->set_url(GURL("https://sub.a.com/final"));
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillRedirectRequest());
  EXPECT_EQ(content::NavigationThrottle::PROCEED,
            throttle->WillProcessResponse());
  EXPECT_FALSE(HasRecordedUseCounter());
}
