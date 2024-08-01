// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_CHILD_NAVIGATION_THROTTLE_H_
#define COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_CHILD_NAVIGATION_THROTTLE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/content/shared/browser/child_frame_navigation_filtering_throttle.h"

class GURL;

namespace content {
class NavigationHandle;
}  // namespace content

namespace subresource_filter {
class AsyncDocumentSubresourceFilter;
}  // namespace subresource_filter

namespace fingerprinting_protection_filter {

// `ChildFrameNavigationFilteringThrottle` implementation for Fingerprinting
// Protection.
class FingerprintingProtectionChildNavigationThrottle
    : public subresource_filter::ChildFrameNavigationFilteringThrottle {
 public:
  FingerprintingProtectionChildNavigationThrottle(
      content::NavigationHandle* handle,
      subresource_filter::AsyncDocumentSubresourceFilter* parent_frame_filter,
      base::RepeatingCallback<std::string(const GURL& url)>
          disallow_message_callback);

  FingerprintingProtectionChildNavigationThrottle(
      const FingerprintingProtectionChildNavigationThrottle&) = delete;
  FingerprintingProtectionChildNavigationThrottle& operator=(
      const FingerprintingProtectionChildNavigationThrottle&) = delete;

  ~FingerprintingProtectionChildNavigationThrottle() override;

  const char* GetNameForLogging() override;

 private:
  bool ShouldDeferNavigation() const override;
  void OnReadyToResumeNavigationWithLoadPolicy() override;
  void NotifyLoadPolicy() const override;

  base::WeakPtrFactory<FingerprintingProtectionChildNavigationThrottle>
      weak_ptr_factory_{this};
};

}  // namespace fingerprinting_protection_filter

#endif  // COMPONENTS_FINGERPRINTING_PROTECTION_FILTER_BROWSER_FINGERPRINTING_PROTECTION_CHILD_NAVIGATION_THROTTLE_H_
