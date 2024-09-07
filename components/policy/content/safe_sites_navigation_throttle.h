// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_SAFE_SITES_NAVIGATION_THROTTLE_H_
#define COMPONENTS_POLICY_CONTENT_SAFE_SITES_NAVIGATION_THROTTLE_H_

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/content/proceed_until_response_navigation_throttle.h"
#include "content/public/browser/navigation_throttle.h"

class SafeSearchService;

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

// SafeSitesNavigationThrottle provides a simple way to block a navigation
// based on the Safe Search API. The URL is checked against the Safe Search API.
// The check may be asynchronous if the result hasn't been cached yet.
// This class does not check the SafeSitesFilterBehavior policy.
class SafeSitesNavigationThrottle
    : public ProceedUntilResponseNavigationThrottle::Client {
 public:
  SafeSitesNavigationThrottle(content::NavigationHandle* navigation_handle,
                              content::BrowserContext* context,
                              std::optional<std::string_view>
                                  safe_sites_error_page_content = std::nullopt);
  SafeSitesNavigationThrottle(const SafeSitesNavigationThrottle&) = delete;
  SafeSitesNavigationThrottle& operator=(const SafeSitesNavigationThrottle&) =
      delete;
  ~SafeSitesNavigationThrottle() override;

  // ProceedUntilResponseNavigationThrottle::Client overrides.
  void SetDeferredResultCallback(
      const ProceedUntilResponseNavigationThrottle::DeferredResultCallback&
          deferred_result_callback) override;

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  // Callback from SafeSearchService.
  void CheckSafeSearchCallback(bool is_safe);

  // The default implementation DeferredResultCallback.
  void OnDeferredResult(bool proceed,
                        std::optional<ThrottleCheckResult> result);

  // Creates the result to be returned when navigation is canceled.
  ThrottleCheckResult CreateCancelResult() const;

  raw_ptr<SafeSearchService, DanglingUntriaged> safe_search_service_;

  ProceedUntilResponseNavigationThrottle::DeferredResultCallback
      deferred_result_callback_;

  // HTML to be displayed when navigation is canceled by the Safe Sites filter.
  // If null, a default error page will be displayed.
  const std::optional<std::string> safe_sites_error_page_content_;

  // Whether the request was deferred in order to check the Safe Search API.
  bool deferred_ = false;

  // Whether the Safe Search API callback determined the in-progress navigation
  // should be canceled.
  bool should_cancel_ = false;

  base::WeakPtrFactory<SafeSitesNavigationThrottle> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_POLICY_CONTENT_SAFE_SITES_NAVIGATION_THROTTLE_H_
