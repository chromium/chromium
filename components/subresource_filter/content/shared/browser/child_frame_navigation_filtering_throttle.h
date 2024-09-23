// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_CHILD_FRAME_NAVIGATION_FILTERING_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_CHILD_FRAME_NAVIGATION_FILTERING_THROTTLE_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace features {
BASE_DECLARE_FEATURE(kSendCnameAliasesToSubresourceFilterFromBrowser);
}  // namespace features

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;

// Interface for a NavigationThrottle responsible for filtering document loads
// within child frames (subframes and fenced frame main frames), which are
// considered subresource loads of their parent frame and hence are subject to
// subresource filtering using the parent frame's
// AsyncDocumentSubresourceFilter.
//
// The throttle should only be instantiated for navigations occuring in child
// frames owned by documents which already have filtering activated, and
// therefore an associated (Async)DocumentSubresourceFilter.
class ChildFrameNavigationFilteringThrottle
    : public content::NavigationThrottle {
 public:
  ChildFrameNavigationFilteringThrottle(
      content::NavigationHandle* handle,
      AsyncDocumentSubresourceFilter* parent_frame_filter,
      bool alias_check_enabled,
      base::RepeatingCallback<std::string(const GURL& url)>
          disallow_message_callback);

  ChildFrameNavigationFilteringThrottle(
      const ChildFrameNavigationFilteringThrottle&) = delete;
  ChildFrameNavigationFilteringThrottle& operator=(
      const ChildFrameNavigationFilteringThrottle&) = delete;

  ~ChildFrameNavigationFilteringThrottle() override;

  // content::NavigationThrottle:
  const char* GetNameForLogging() override = 0;
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;

 protected:
  enum class DeferStage {
    kNotDeferring,
    kWillStartOrRedirectRequest,
    kWillProcessResponse
  };

  // Defines the conditions under which navigations should be deferred to wait
  // for some computation to complete.
  virtual bool ShouldDeferNavigation() const = 0;

  // Defines any necessary custom logic to execute after a LoadPolicy has been
  // computed for navigations that will *not* be cancelled.
  virtual void OnReadyToResumeNavigationWithLoadPolicy() = 0;

  // Notifies the appropriate observer that the
  // ChildFrameNavigationFilteringThrottle has finished processing a request.
  // Called by either WillProcessResponse(), CancelNavigation(), or
  // ResumeNavigation() as the last processing step.
  virtual void NotifyLoadPolicy() const = 0;

  content::NavigationThrottle::ThrottleCheckResult
  MaybeDeferToCalculateLoadPolicy();

  void OnCalculatedLoadPolicy(LoadPolicy policy);
  void OnCalculatedLoadPolicyForUrl(LoadPolicy policy);
  void OnCalculatedLoadPoliciesFromAliasUrls(std::vector<LoadPolicy> policies);
  void HandleDisallowedLoad();

  void DeferStart(DeferStage stage);
  void UpdateDeferInfo();

  void CancelNavigation();
  void ResumeNavigation();

  // Must outlive this class.
  raw_ptr<AsyncDocumentSubresourceFilter, DanglingUntriaged>
      parent_frame_filter_;

  int pending_load_policy_calculations_ = 0;
  DeferStage defer_stage_ = DeferStage::kNotDeferring;

  // Time tracking for metrics collection.
  base::TimeTicks last_defer_timestamp_;
  base::TimeDelta total_defer_time_;

  const bool alias_check_enabled_;

  // Set to true if aliases were checked.
  bool did_alias_check_ = false;

  // Set to true if alias checking determined the load policy. If the non-alias
  // check and the alias check are the same load policy, then whichever check
  // came first will determine the value of this variable.
  bool did_alias_check_determine_load_policy_ = false;

  // Set to the least restrictive load policy by default.
  LoadPolicy load_policy_ = LoadPolicy::EXPLICITLY_ALLOW;

  // Callback to construct a console message based on a disallowed resource URL
  // without storing a copy of the message string.
  base::RepeatingCallback<std::string(const GURL& gurl)>
      disallow_message_callback_;

  base::WeakPtrFactory<ChildFrameNavigationFilteringThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_CHILD_FRAME_NAVIGATION_FILTERING_THROTTLE_H_
