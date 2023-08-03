// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CHILD_FRAME_NAVIGATION_FILTERING_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CHILD_FRAME_NAVIGATION_FILTERING_THROTTLE_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "content/public/browser/navigation_throttle.h"

namespace features {
BASE_DECLARE_FEATURE(kSendCnameAliasesToSubresourceFilterFromBrowser);
}  // namespace features

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;

// NavigationThrottle responsible for filtering child frame (subframes and
// fenced frame main frames) document loads, which are considered subresource
// loads of their parent frame, hence are subject to subresource filtering using
// the parent frame's AsyncDocumentSubresourceFilter.
//
// The throttle should only be instantiated for navigations occuring in child
// frames owned by documents which already have filtering activated, and
// therefore an associated (Async)DocumentSubresourceFilter.
//
// TODO(https://crbug.com/984562): With AdTagging enabled, this throttle delays
// almost all child frame navigations. This delay is necessary in blocking mode
// due to logic related to BLOCK_REQUEST_AND_COLLAPSE. However, there may be
// room for optimization during AdTagging, or migrating
// BLOCK_REQUEST_AND_COLLAPSE to be allowed during WillProcessResponse.
class ChildFrameNavigationFilteringThrottle
    : public content::NavigationThrottle {
 public:
  ChildFrameNavigationFilteringThrottle(
      content::NavigationHandle* handle,
      AsyncDocumentSubresourceFilter* parent_frame_filter);

  ChildFrameNavigationFilteringThrottle(
      const ChildFrameNavigationFilteringThrottle&) = delete;
  ChildFrameNavigationFilteringThrottle& operator=(
      const ChildFrameNavigationFilteringThrottle&) = delete;

  ~ChildFrameNavigationFilteringThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  enum class DeferStage {
    kNotDeferring,
    kWillStartOrRedirectRequest,
    kWillProcessResponse
  };

  content::NavigationThrottle::ThrottleCheckResult
  MaybeDeferToCalculateLoadPolicy();

  void OnCalculatedLoadPolicy(LoadPolicy policy);
  void OnCalculatedLoadPoliciesFromAliasUrls(std::vector<LoadPolicy> policies);
  void HandleDisallowedLoad();

  void NotifyLoadPolicy() const;

  void DeferStart(DeferStage stage);
  void UpdateDeferInfo();

  void CancelNavigation();
  void ResumeNavigation();

  // Must outlive this class.
  raw_ptr<AsyncDocumentSubresourceFilter, DanglingUntriaged>
      parent_frame_filter_;

  int pending_load_policy_calculations_ = 0;
  DeferStage defer_stage_ = DeferStage::kNotDeferring;
  base::TimeTicks last_defer_timestamp_;
  base::TimeDelta total_defer_time_;

  const bool alias_check_enabled_;

  // Set to the least restrictive load policy by default.
  LoadPolicy load_policy_ = LoadPolicy::EXPLICITLY_ALLOW;

  base::WeakPtrFactory<ChildFrameNavigationFilteringThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CHILD_FRAME_NAVIGATION_FILTERING_THROTTLE_H_
