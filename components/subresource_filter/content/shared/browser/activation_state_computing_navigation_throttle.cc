// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/shared/browser/activation_state_computing_navigation_throttle.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "components/subresource_filter/content/shared/browser/utils.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace subresource_filter {

// static
std::unique_ptr<ActivationStateComputingNavigationThrottle>
ActivationStateComputingNavigationThrottle::CreateForRoot(
    content::NavigationHandle* navigation_handle) {
  CHECK(IsInSubresourceFilterRoot(navigation_handle),
        base::NotFatalUntil::M129);
  return base::WrapUnique(new ActivationStateComputingNavigationThrottle(
      navigation_handle, /*parent_activation_state=*/std::nullopt,
      /*ruleset_handle*/ nullptr));
}

// static
std::unique_ptr<ActivationStateComputingNavigationThrottle>
ActivationStateComputingNavigationThrottle::CreateForChild(
    content::NavigationHandle* navigation_handle,
    VerifiedRuleset::Handle* ruleset_handle,
    const mojom::ActivationState& parent_activation_state) {
  CHECK(!IsInSubresourceFilterRoot(navigation_handle),
        base::NotFatalUntil::M129);
  CHECK_NE(mojom::ActivationLevel::kDisabled,
           parent_activation_state.activation_level, base::NotFatalUntil::M129);
  CHECK(ruleset_handle, base::NotFatalUntil::M129);
  return base::WrapUnique(new ActivationStateComputingNavigationThrottle(
      navigation_handle, parent_activation_state, ruleset_handle));
}

ActivationStateComputingNavigationThrottle::
    ActivationStateComputingNavigationThrottle(
        content::NavigationHandle* navigation_handle,
        const std::optional<mojom::ActivationState> parent_activation_state,
        VerifiedRuleset::Handle* ruleset_handle)
    : content::NavigationThrottle(navigation_handle),
      parent_activation_state_(parent_activation_state),
      ruleset_handle_(ruleset_handle ? ruleset_handle->AsWeakPtr() : nullptr) {}

ActivationStateComputingNavigationThrottle::
    ~ActivationStateComputingNavigationThrottle() = default;

void ActivationStateComputingNavigationThrottle::
    NotifyPageActivationWithRuleset(
        VerifiedRuleset::Handle* ruleset_handle,
        const mojom::ActivationState& page_activation_state) {
  CHECK(IsInSubresourceFilterRoot(navigation_handle()),
        base::NotFatalUntil::M129);
  CHECK_NE(mojom::ActivationLevel::kDisabled,
           page_activation_state.activation_level, base::NotFatalUntil::M129);
  parent_activation_state_ = page_activation_state;
  CHECK(ruleset_handle, base::NotFatalUntil::M129);
  ruleset_handle_ = ruleset_handle->AsWeakPtr();
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillStartRequest() {
  if (parent_activation_state_)
    CheckActivationState();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillRedirectRequest() {
  if (parent_activation_state_)
    CheckActivationState();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
ActivationStateComputingNavigationThrottle::WillProcessResponse() {
  // If no parent activation, this is root frame that was never notified of
  // activation.
  if (!parent_activation_state_) {
    CHECK(IsInSubresourceFilterRoot(navigation_handle()),
          base::NotFatalUntil::M129);
    CHECK(!async_filter_, base::NotFatalUntil::M129);
    CHECK(!ruleset_handle_, base::NotFatalUntil::M129);
    return content::NavigationThrottle::PROCEED;
  }

  // Throttles which have finished their last check should just proceed here.
  // All others need to defer and either wait for their existing check to
  // finish, or start a new check now if there was no previous speculative
  // check.
  if (async_filter_ && async_filter_->has_activation_state()) {
    if (IsInSubresourceFilterRoot(navigation_handle()))
      UpdateWithMoreAccurateState();
    return content::NavigationThrottle::PROCEED;
  }
  CHECK(!deferred_, base::NotFatalUntil::M129);
  deferred_ = true;
  if (!async_filter_) {
    CHECK(IsInSubresourceFilterRoot(navigation_handle()),
          base::NotFatalUntil::M129);
    CheckActivationState();
  }
  return content::NavigationThrottle::DEFER;
}

const char* ActivationStateComputingNavigationThrottle::GetNameForLogging() {
  return "ActivationStateComputingNavigationThrottle";
}

void ActivationStateComputingNavigationThrottle::CheckActivationState() {
  CHECK(parent_activation_state_, base::NotFatalUntil::M129);
  CHECK(ruleset_handle_, base::NotFatalUntil::M129);
  AsyncDocumentSubresourceFilter::InitializationParams params;
  params.document_url = navigation_handle()->GetURL();
  params.parent_activation_state = parent_activation_state_.value();
  if (!IsInSubresourceFilterRoot(navigation_handle())) {
    content::RenderFrameHost* parent =
        navigation_handle()->GetParentFrameOrOuterDocument();
    CHECK(parent, base::NotFatalUntil::M129);
    params.parent_document_origin = parent->GetLastCommittedOrigin();
  }

  // If there is an existing |async_filter_| that hasn't called
  // OnActivationStateComputed, it will be deleted here and never call that
  // method. This is by design of the AsyncDocumentSubresourceFilter, which
  // will drop the message via weak pointer semantics.
  async_filter_ = std::make_unique<AsyncDocumentSubresourceFilter>(
      ruleset_handle_.get(), std::move(params),
      base::BindOnce(&ActivationStateComputingNavigationThrottle::
                         OnActivationStateComputed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ActivationStateComputingNavigationThrottle::OnActivationStateComputed(
    mojom::ActivationState state) {
  if (deferred_) {
    if (IsInSubresourceFilterRoot(navigation_handle()))
      UpdateWithMoreAccurateState();
    Resume();
  }
}

void ActivationStateComputingNavigationThrottle::UpdateWithMoreAccurateState() {
  // This method is only needed for root frame navigations that are notified of
  // page activation more than once. Even for those that are updated once, it
  // should be a no-op.
  CHECK(IsInSubresourceFilterRoot(navigation_handle()),
        base::NotFatalUntil::M129);
  CHECK(parent_activation_state_, base::NotFatalUntil::M129);
  CHECK(async_filter_, base::NotFatalUntil::M129);
  async_filter_->UpdateWithMoreAccurateState(*parent_activation_state_);
}

AsyncDocumentSubresourceFilter*
ActivationStateComputingNavigationThrottle::filter() const {
  // TODO(csharrison): This should not really be necessary, as we should be
  // delaying the navigation until the filter has computed an activation state.
  // See crbug.com/736249. In the mean time, have a check here to avoid
  // returning a filter in an invalid state.
  if (async_filter_ && async_filter_->has_activation_state())
    return async_filter_.get();
  return nullptr;
}

// Ensure the caller cannot take ownership of a subresource filter for cases
// when activation IPCs are not sent to the render process.
std::unique_ptr<AsyncDocumentSubresourceFilter>
ActivationStateComputingNavigationThrottle::ReleaseFilter() {
  return will_send_activation_to_renderer_ ? std::move(async_filter_) : nullptr;
}

void ActivationStateComputingNavigationThrottle::
    WillSendActivationToRenderer() {
  CHECK(async_filter_, base::NotFatalUntil::M129);
  will_send_activation_to_renderer_ = true;
}

}  // namespace subresource_filter
