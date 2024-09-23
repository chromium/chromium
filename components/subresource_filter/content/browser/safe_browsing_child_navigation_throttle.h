// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_CHILD_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_CHILD_NAVIGATION_THROTTLE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_filtering_throttle.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"

class GURL;

namespace content {
class NavigationThrottle;
}
namespace subresource_filter {

class AsyncDocumentSubresourceFilter;
class ProfileInteractionManager;

// ChildFrameNavigationFilteringThrottle implementation for Safe Browsing.
//
// TODO(https://crbug.com/41471110): With AdTagging enabled, this throttle
// delays almost all child frame navigations. This delay is necessary in
// blocking mode due to logic related to BLOCK_REQUEST_AND_COLLAPSE. However,
// there may be room for optimization during AdTagging, or migrating
// BLOCK_REQUEST_AND_COLLAPSE to be allowed during WillProcessResponse.
class SafeBrowsingChildNavigationThrottle
    : public ChildFrameNavigationFilteringThrottle {
 public:
  SafeBrowsingChildNavigationThrottle(
      content::NavigationHandle* handle,
      AsyncDocumentSubresourceFilter* parent_frame_filter,
      base::WeakPtr<ProfileInteractionManager> profile_interaction_manager,
      base::RepeatingCallback<std::string(const GURL& url)>
          disallow_message_callback,
      std::optional<blink::FrameAdEvidence> ad_evidence);

  SafeBrowsingChildNavigationThrottle(
      const SafeBrowsingChildNavigationThrottle&) = delete;
  SafeBrowsingChildNavigationThrottle& operator=(
      const SafeBrowsingChildNavigationThrottle&) = delete;

  ~SafeBrowsingChildNavigationThrottle() override;

  const char* GetNameForLogging() override;

 private:
  bool ShouldDeferNavigation() const override;
  void OnReadyToResumeNavigationWithLoadPolicy() override;
  void NotifyLoadPolicy() const override;
  bool NavigationHasCookieException() const;

  std::optional<blink::FrameAdEvidence> ad_evidence_;

  // May be null. If non-null, must outlive this class.
  base::WeakPtr<ProfileInteractionManager> profile_interaction_manager_;

  base::WeakPtrFactory<SafeBrowsingChildNavigationThrottle> weak_ptr_factory_{
      this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SAFE_BROWSING_CHILD_NAVIGATION_THROTTLE_H_
