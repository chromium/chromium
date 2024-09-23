// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_CHILD_FRAME_NAVIGATION_TEST_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_CHILD_FRAME_NAVIGATION_TEST_UTILS_H_

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {
class NavigationSimulator;
}

namespace subresource_filter {

content::NavigationThrottle::ThrottleCheckResult SimulateStartAndGetResult(
    content::NavigationSimulator* navigation_simulator);

content::NavigationThrottle::ThrottleCheckResult SimulateRedirectAndGetResult(
    content::NavigationSimulator* navigation_simulator,
    const GURL& new_url);

content::NavigationThrottle::ThrottleCheckResult SimulateCommitAndGetResult(
    content::NavigationSimulator* navigation_simulator);

void SimulateFailedNavigation(
    content::NavigationSimulator* navigation_simulator,
    net::Error error);

// Harness class that implements most functionality needed to test different
// implementations of ChildFrameNavigationFilteringThrottle and can be reused
// for unittests.
class ChildFrameNavigationFilteringThrottleTestHarness
    : public content::RenderViewHostTestHarness,
      public content::WebContentsObserver {
 public:
  ChildFrameNavigationFilteringThrottleTestHarness();

  ChildFrameNavigationFilteringThrottleTestHarness(
      const ChildFrameNavigationFilteringThrottleTestHarness&) = delete;
  ChildFrameNavigationFilteringThrottleTestHarness& operator=(
      const ChildFrameNavigationFilteringThrottleTestHarness&) = delete;

  ~ChildFrameNavigationFilteringThrottleTestHarness() override;

  void SetUp() override;

  void TearDown() override;

  content::NavigationSimulator* navigation_simulator() {
    return navigation_simulator_.get();
  }

  // From content::WebContentsObserver. Should be overridden to inject
  // the proper ChildFrameNavigationFilteringThrottle implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override = 0;

  // Should be overridden to return the proper console message for the given
  // ChildFrameNavigationFilteringThrottle implementation.
  virtual std::string GetFilterConsoleMessage(const GURL& filtered_url);

  void InitializeDocumentSubresourceFilter(
      const GURL& document_url,
      mojom::ActivationLevel parent_level = mojom::ActivationLevel::kEnabled);

  void InitializeDocumentSubresourceFilterWithSubstringRules(
      const GURL& document_url,
      std::vector<std::string_view> urls_to_block,
      mojom::ActivationLevel parent_level = mojom::ActivationLevel::kEnabled);

  void RunUntilIdle();

  void CreateTestSubframeAndInitNavigation(const GURL& first_url,
                                           content::RenderFrameHost* parent);

  const std::vector<std::string>& GetConsoleMessages();

  void SetResponseDnsAliasesForNavigation(std::vector<std::string> aliases);

 protected:
  void FinishInitializingDocumentSubresourceFilter(
      const GURL& document_url,
      mojom::ActivationLevel parent_level);

  testing::TestRulesetCreator test_ruleset_creator_;
  testing::TestRulesetPair test_ruleset_pair_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> dealer_handle_;
  std::unique_ptr<VerifiedRuleset::Handle> ruleset_handle_;

  std::unique_ptr<AsyncDocumentSubresourceFilter> parent_filter_;

  std::unique_ptr<content::NavigationSimulator> navigation_simulator_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_CHILD_FRAME_NAVIGATION_TEST_UTILS_H_
