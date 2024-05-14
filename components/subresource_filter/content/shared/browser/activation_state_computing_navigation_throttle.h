// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_ACTIVATION_STATE_COMPUTING_NAVIGATION_THROTTLE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_ACTIVATION_STATE_COMPUTING_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_throttle.h"

namespace subresource_filter {

class AsyncDocumentSubresourceFilter;

// NavigationThrottle responsible for determining the activation state of
// subresource filtering for a given navigation (either in the root frame or in
// a child frame); and for deferring that navigation at WillProcessResponse
// until the activation state computation on the ruleset's task runner is
// complete.
//
// Interested parties can retrieve the activation state after this point (most
// likely in ReadyToCommitNavigation).
//
// Note: for performance, activation computation for child frames is done
// speculatively at navigation start and at every redirect. This is to reduce
// the wait time (most likely to 0) by WillProcessResponse time. For main
// frames, speculation will be done at the next navigation stage after
// NotifyPageActivationWithRuleset is called.
class ActivationStateComputingNavigationThrottle
    : public content::NavigationThrottle {
 public:
  // For subresource filter root frames, a verified ruleset handle is not
  // readily available at construction time. Since it is expensive to "warm up"
  // the ruleset, the ruleset handle will be injected in
  // NotifyPageActivationWithRuleset once it has been established that
  // activation computation is needed.
  static std::unique_ptr<ActivationStateComputingNavigationThrottle>
  CreateForRoot(content::NavigationHandle* navigation_handle);

  // It is illegal to create an activation computing throttle for frames
  // whose parents are not activated. Similarly, |ruleset_handle| should be
  // non-null.
  static std::unique_ptr<ActivationStateComputingNavigationThrottle>
  CreateForChild(content::NavigationHandle* navigation_handle,
                 VerifiedRuleset::Handle* ruleset_handle,
                 const mojom::ActivationState& parent_activation_state);

  ActivationStateComputingNavigationThrottle(
      const ActivationStateComputingNavigationThrottle&) = delete;
  ActivationStateComputingNavigationThrottle& operator=(
      const ActivationStateComputingNavigationThrottle&) = delete;

  ~ActivationStateComputingNavigationThrottle() override;

  // Notification for root frames when the page level activation is computed.
  // Must be called at most once before WillProcessResponse is called on this
  // throttle. If it is never called, this object will never delay the
  // navigation for root frames.
  //
  // Should never be called with DISABLED activation.
  //
  // Note: can be called multiple times, at any point in the navigation to
  // update the page state. |page_activation_state| will be merged into any
  // previously computed activation state.
  void NotifyPageActivationWithRuleset(
      VerifiedRuleset::Handle* ruleset_handle,
      const mojom::ActivationState& page_activation_state);

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

  // After the navigation is finished, the client may optionally choose to
  // continue using the DocumentSubresourceFilter that was used to compute the
  // activation state for this frame. The transferred filter can be cached and
  // used to calculate load policy for child frame navigations occurring in this
  // frame.
  std::unique_ptr<AsyncDocumentSubresourceFilter> ReleaseFilter();

  AsyncDocumentSubresourceFilter* filter() const;

  void WillSendActivationToRenderer();

 private:
  void CheckActivationState();
  void OnActivationStateComputed(mojom::ActivationState state);

  // In the case when root frame navigations get notified of
  // mojom::ActivationState multiple times, a method is needed for overriding
  // previously computed results with a more accurate mojom::ActivationState.
  //
  // This must be called at the end of the WillProcessResponse stage.
  void UpdateWithMoreAccurateState();

  ActivationStateComputingNavigationThrottle(
      content::NavigationHandle* navigation_handle,
      const std::optional<mojom::ActivationState> parent_activation_state,
      VerifiedRuleset::Handle* ruleset_handle);

  // Optional to allow for CHECKing.
  std::optional<mojom::ActivationState> parent_activation_state_;

  std::unique_ptr<AsyncDocumentSubresourceFilter> async_filter_;

  // For root frame navigations, this member will be nullptr until
  // NotifyPageActivationWithRuleset is called.
  base::WeakPtr<VerifiedRuleset::Handle> ruleset_handle_;

  // Will be set to true when DEFER is called in WillProcessResponse.
  bool deferred_ = false;

  // Will become true when the throttle manager reaches ReadyToCommitNavigation.
  // Makes sure a caller cannot take ownership of the subresource filter unless
  // the throttle has reached this point. After this point the throttle manager
  // will send an activation IPC to the render process.
  bool will_send_activation_to_renderer_ = false;

  base::WeakPtrFactory<ActivationStateComputingNavigationThrottle>
      weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_ACTIVATION_STATE_COMPUTING_NAVIGATION_THROTTLE_H_
