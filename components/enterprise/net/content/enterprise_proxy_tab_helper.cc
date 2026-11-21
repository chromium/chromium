// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/content/enterprise_proxy_tab_helper.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace enterprise_net {

DEFINE_USER_DATA(EnterpriseProxyTabHelper);

EnterpriseProxyTabHelper::EnterpriseProxyTabHelper(
    tabs::TabInterface& tab,
    content::WebContents* web_contents,
    EnterpriseProxyErrorService* error_service,
    std::unique_ptr<Delegate> delegate)
    : content::WebContentsObserver(web_contents),
      error_service_(error_service),
      delegate_(std::move(delegate)),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  // `error_service_` can be null (e.g., in Incognito/Guest profiles, when
  // dynamic route fetching is disabled, or in tests without a testing
  // factory).
  if (error_service_) {
    error_service_observation_.Observe(error_service_);
  }
}

EnterpriseProxyTabHelper::~EnterpriseProxyTabHelper() = default;

// static
EnterpriseProxyTabHelper* EnterpriseProxyTabHelper::From(
    tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

void EnterpriseProxyTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    active_navigation_id_ = navigation_handle->GetNavigationId();
    active_navigation_url_ = navigation_handle->GetURL();
  }
}

void EnterpriseProxyTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->GetNavigationId() == active_navigation_id_) {
    active_navigation_url_ = navigation_handle->GetURL();
  }
}

void EnterpriseProxyTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    const int64_t finished_id = navigation_handle->GetNavigationId();
    if (error_service_) {
      error_service_->RemoveDisguisedError(finished_id);
    }
    if (active_navigation_id_ == finished_id) {
      active_navigation_id_ = 0;
      active_navigation_url_ = GURL();
    }
    if (error_redirect_navigation_id_ == finished_id) {
      error_redirect_navigation_id_ = 0;
    }
  }
}

void EnterpriseProxyTabHelper::OnDisguisedErrorRecorded(
    int64_t navigation_id,
    const EnterpriseProxyErrorData& error_data) {
  if (!IsStaticErrorPageRedirectEnabled() || navigation_id == 0 ||
      navigation_id != active_navigation_id_ || !error_service_) {
    return;
  }

  const std::optional<GURL>& redirect_url = error_data.redirect_url();
  if (!redirect_url.has_value()) {
    return;
  }

  // Re-validate at the navigation sink (`EnterpriseProxyErrorData` is publicly
  // constructible). Embedded `user:pass@` credentials in URLs are rejected
  // because they are deprecated (RFC 9110) and can pre-seed HTTP auth
  // credentials, trigger deceptive-URL warnings, or obscure the destination
  // host (`https://trusted.example@evil.example/...`).
  if (!redirect_url->is_valid() || !redirect_url->SchemeIs(url::kHttpsScheme) ||
      redirect_url->has_username() || redirect_url->has_password()) {
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::kRejectedInvalidUrl);
    return;
  }

  if (!active_navigation_url_.EqualsIgnoringRef(error_data.destination_url())) {
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::
            kRejectedDestinationMismatch);
    return;
  }

  // Prevent redirect loops: if the static error page navigation itself hits
  // another disguised-error `url=`, or if `redirect_url` points back to the
  // exact destination that just failed, leave `error_data` in `error_service_`
  // so Chrome falls back to the built-in error interstitial.
  if (navigation_id == error_redirect_navigation_id_ ||
      redirect_url->EqualsIgnoringRef(error_data.destination_url())) {
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::kRejectedLoopDetected);
    return;
  }

  const GURL target_url = *redirect_url;
  error_service_->RemoveDisguisedError(navigation_id);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&EnterpriseProxyTabHelper::RedirectToStaticErrorPage,
                     weak_ptr_factory_.GetWeakPtr(), navigation_id,
                     target_url));
}

void EnterpriseProxyTabHelper::OnEnterpriseProxyErrorServiceShuttingDown() {
  error_service_observation_.Reset();
  error_service_ = nullptr;
}

void EnterpriseProxyTabHelper::RedirectToStaticErrorPage(
    int64_t failed_navigation_id,
    const GURL& redirect_url) {
  if (!web_contents() || active_navigation_id_ != failed_navigation_id) {
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::kCanceledBeforeRedirect);
    return;
  }

  // Clear before starting the replacement navigation so no duplicate redirect
  // task for `failed_navigation_id` can run even if `LoadURLWithParams()`
  // defers `DidStartNavigation()` (e.g. while awaiting `beforeunload`).
  active_navigation_id_ = 0;
  active_navigation_url_ = GURL();

  // Start a clean browser-initiated GET navigation (`is_renderer_initiated =
  // false`) with an empty Referrer, no POST data or extra headers, and an
  // unconditional opaque `initiator_origin` (`url::Origin()`):
  // - Keeping `is_renderer_initiated = false` prevents
  //   `NavigationControllerImpl::SetShouldSkipOnBackForwardUIIfNeeded()` from
  //   blaming the previous committed page for a gestureless redirect and
  //   marking it skippable on Back when the 407 navigation never committed.
  // - Passing an explicit opaque `url::Origin()` prevents
  //   `ComputeSameSiteContextForRequest()` from attaching `SameSite=Strict`
  //   cookies (even when `redirect_url` is same-site with the proxy) and sends
  //   `Sec-Fetch-Site: cross-site` instead of `none`.
  content::NavigationController::LoadURLParams load_params(redirect_url);
  load_params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  load_params.is_renderer_initiated = false;
  load_params.initiator_origin = url::Origin();
  base::WeakPtr<content::NavigationHandle> handle =
      web_contents()->GetController().LoadURLWithParams(load_params);
  if (handle) {
    error_redirect_navigation_id_ = handle->GetNavigationId();
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::kRedirected);
  } else {
    RecordStaticErrorPageRedirectOutcome(
        SecureGatewayStaticErrorPageRedirectOutcome::kCanceledBeforeRedirect);
  }
}

void EnterpriseProxyTabHelper::SignIn() {
  if (delegate_ && web_contents()) {
    delegate_->SignIn(web_contents());
  }
}

}  // namespace enterprise_net
