// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_FILTERING_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_FILTERING_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/common/load_policy.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;

// NavigationThrottle responsible for filtering subframe document loads, which
// are considered subresource loads of their parent frame, hence are subject to
// subresource filtering using the parent frame's
// AsyncDocumentSubresourceFilter.
//
// The throttle should only be instantiated for navigations occuring in
// subframes owned by documents which already have filtering activated, and
// therefore an associated (Async)DocumentSubresourceFilter.
//
// TODO(https://crbug.com/984562): With AdTagging enabled, this throttle delays
// almost all subframe navigations. This delay is necessary in blocking mode due
// to logic related to BLOCK_REQUEST_AND_COLLAPSE. However, there may be room
// for optimization during AdTagging, or migrating BLOCK_REQUEST_AND_COLLAPSE to
// be allowed during WillProcessResponse.
class SubframeNavigationFilteringThrottle : public content::NavigationThrottle {
 public:
  class Delegate {
   public:
    // Given what is known about the frame's load policy, its parent frame, and
    // what it's learned from ad tagging, determine if it's an ad subframe.
    virtual bool CalculateIsAdSubframe(content::RenderFrameHost* frame_host,
                                       LoadPolicy load_policy) = 0;

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // |delegate| must outlive this object.
  SubframeNavigationFilteringThrottle(
      content::NavigationHandle* handle,
      AsyncDocumentSubresourceFilter* parent_frame_filter,
      Delegate* delegate);
  ~SubframeNavigationFilteringThrottle() override;

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
  void HandleDisallowedLoad();

  void NotifyLoadPolicy() const;

  void DeferStart(DeferStage stage);

  // Must outlive this class.
  AsyncDocumentSubresourceFilter* parent_frame_filter_;

  int pending_load_policy_calculations_ = 0;
  DeferStage defer_stage_ = DeferStage::kNotDeferring;
  base::TimeTicks last_defer_timestamp_;
  base::TimeDelta total_defer_time_;
  LoadPolicy load_policy_ = LoadPolicy::ALLOW;

  // As specified in the constructor comment, |delegate_| must outlive this
  // object.
  Delegate* delegate_;

  base::WeakPtrFactory<SubframeNavigationFilteringThrottle> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SubframeNavigationFilteringThrottle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBFRAME_NAVIGATION_FILTERING_THROTTLE_H_
