// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_SAFE_SITES_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_SAFE_SITES_NAVIGATION_THROTTLE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "content/public/browser/navigation_throttle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SafeSearchService;

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// SafeSitesNavigationThrottle provides a simple way to block a navigation
// based on the Safe Search API. The URL is checked against the Safe Search API.
// The check may be asynchronous if the result hasn't been cached yet.
// This class does not check the SafeSitesFilterBehavior policy.
class SafeSitesNavigationThrottle : public content::NavigationThrottle {
 public:
  // Called when the SafeSearch result is available after being deferred.
  // if !|is_safe|, |cancel_result| contains the result to pass to
  // CancelDeferredNavigation(). Other NavigationThrottles using this object
  // must handle resolving deferred navigation themselves due to checks in
  // NavigationThrottleRunner.
  using DeferredResultCallback =
      base::RepeatingCallback<void(bool is_safe,
                                   ThrottleCheckResult cancel_result)>;

  SafeSitesNavigationThrottle(content::NavigationHandle* navigation_handle,
                              content::BrowserContext* context);
  SafeSitesNavigationThrottle(content::NavigationHandle* navigation_handle,
                              content::BrowserContext* context,
                              DeferredResultCallback deferred_result_callback);
  SafeSitesNavigationThrottle(content::NavigationHandle* navigation_handle,
                              content::BrowserContext* context,
                              base::StringPiece safe_sites_error_page_content);
  SafeSitesNavigationThrottle(const SafeSitesNavigationThrottle&) = delete;
  SafeSitesNavigationThrottle& operator=(const SafeSitesNavigationThrottle&) =
      delete;
  ~SafeSitesNavigationThrottle() override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Callback from SafeSearchService.
  void CheckSafeSearchCallback(bool is_safe);

  // The default implementation DeferredResultCallback.
  void OnDeferredResult(bool is_safe, ThrottleCheckResult cancel_result);

  // Creates the result to be returned when navigation is canceled.
  ThrottleCheckResult CreateCancelResult() const;

  raw_ptr<SafeSearchService, DanglingUntriaged> safe_seach_service_;

  const DeferredResultCallback deferred_result_callback_;

  // HTML to be displayed when navigation is canceled by the Safe Sites filter.
  // If null, a default error page will be displayed.
  const absl::optional<std::string> safe_sites_error_page_content_;

  // Whether the request was deferred in order to check the Safe Search API.
  bool deferred_ = false;

  // Whether the Safe Search API callback determined the in-progress navigation
  // should be canceled.
  bool should_cancel_ = false;

  base::WeakPtrFactory<SafeSitesNavigationThrottle> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_POLICY_CONTENT_SAFE_SITES_NAVIGATION_THROTTLE_H_
