// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"

#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_filtering_throttle.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_test_utils.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

content::NavigationThrottle::ThrottleCheckResult SimulateStartAndGetResult(
    content::NavigationSimulator* navigation_simulator) {
  navigation_simulator->Start();
  return navigation_simulator->GetLastThrottleCheckResult();
}

content::NavigationThrottle::ThrottleCheckResult SimulateRedirectAndGetResult(
    content::NavigationSimulator* navigation_simulator,
    const GURL& new_url) {
  navigation_simulator->Redirect(new_url);
  return navigation_simulator->GetLastThrottleCheckResult();
}

content::NavigationThrottle::ThrottleCheckResult SimulateCommitAndGetResult(
    content::NavigationSimulator* navigation_simulator) {
  navigation_simulator->Commit();
  return navigation_simulator->GetLastThrottleCheckResult();
}

void SimulateFailedNavigation(
    content::NavigationSimulator* navigation_simulator,
    net::Error error) {
  navigation_simulator->Fail(error);
  if (error != net::ERR_ABORTED) {
    navigation_simulator->CommitErrorPage();
  }
}

ChildFrameNavigationFilteringThrottleTestHarness::
    ChildFrameNavigationFilteringThrottleTestHarness() = default;

ChildFrameNavigationFilteringThrottleTestHarness::
    ~ChildFrameNavigationFilteringThrottleTestHarness() = default;

void ChildFrameNavigationFilteringThrottleTestHarness::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  NavigateAndCommit(GURL("https://example.test"));
  Observe(RenderViewHostTestHarness::web_contents());
}

void ChildFrameNavigationFilteringThrottleTestHarness::TearDown() {
  dealer_handle_.reset();
  ruleset_handle_.reset();
  navigation_simulator_.reset();
  parent_filter_.reset();
  RunUntilIdle();
  content::RenderViewHostTestHarness::TearDown();
}

std::string
ChildFrameNavigationFilteringThrottleTestHarness::GetFilterConsoleMessage(
    const GURL& filtered_url) {
  return base::StringPrintf(kDisallowChildFrameConsoleMessageFormat,
                            filtered_url.possibly_invalid_spec().c_str());
}

void ChildFrameNavigationFilteringThrottleTestHarness::
    InitializeDocumentSubresourceFilter(const GURL& document_url,
                                        mojom::ActivationLevel parent_level) {
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
          "disallowed.html", &test_ruleset_pair_));

  FinishInitializingDocumentSubresourceFilter(document_url, parent_level);
}

void ChildFrameNavigationFilteringThrottleTestHarness::
    InitializeDocumentSubresourceFilterWithSubstringRules(
        const GURL& document_url,
        std::vector<std::string_view> urls_to_block,
        mojom::ActivationLevel parent_level) {
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_creator_.CreateRulesetToDisallowURLWithSubstrings(
          urls_to_block, &test_ruleset_pair_));

  FinishInitializingDocumentSubresourceFilter(document_url, parent_level);
}

void ChildFrameNavigationFilteringThrottleTestHarness::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void ChildFrameNavigationFilteringThrottleTestHarness::
    CreateTestSubframeAndInitNavigation(const GURL& first_url,
                                        content::RenderFrameHost* parent) {
  content::RenderFrameHost* render_frame =
      content::RenderFrameHostTester::For(parent)->AppendChild(
          base::StringPrintf("subframe-%s", first_url.spec().c_str()));
  navigation_simulator_ = content::NavigationSimulator::CreateRendererInitiated(
      first_url, render_frame);
}

const std::vector<std::string>&
ChildFrameNavigationFilteringThrottleTestHarness::GetConsoleMessages() {
  return content::RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
}

void ChildFrameNavigationFilteringThrottleTestHarness::
    SetResponseDnsAliasesForNavigation(std::vector<std::string> aliases) {
  CHECK(navigation_simulator_);
  navigation_simulator_->SetResponseDnsAliases(std::move(aliases));
}

void ChildFrameNavigationFilteringThrottleTestHarness::
    FinishInitializingDocumentSubresourceFilter(
        const GURL& document_url,
        mojom::ActivationLevel parent_level) {
  // Make the blocking task runner run on the current task runner for the
  // tests, to ensure that the NavigationSimulator properly runs all necessary
  // tasks while waiting for throttle checks to finish.
  dealer_handle_ = std::make_unique<VerifiedRulesetDealer::Handle>(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      kSafeBrowsingRulesetConfig);
  dealer_handle_->TryOpenAndSetRulesetFile(test_ruleset_pair_.indexed.path,
                                           /*expected_checksum=*/0,
                                           base::DoNothing());
  ruleset_handle_ =
      std::make_unique<VerifiedRuleset::Handle>(dealer_handle_.get());

  testing::TestActivationStateCallbackReceiver activation_state;
  mojom::ActivationState parent_activation_state;
  parent_activation_state.activation_level = parent_level;
  parent_activation_state.enable_logging = true;
  parent_filter_ = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle_.get(),
      AsyncDocumentSubresourceFilter::InitializationParams(
          document_url, url::Origin::Create(document_url),
          parent_activation_state),
      activation_state.GetCallback());
  RunUntilIdle();
  activation_state.ExpectReceivedOnce(parent_activation_state);
}

}  // namespace subresource_filter
