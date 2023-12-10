// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/insecure_form_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/insecure_form_tab_storage.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/insecure_form_util.h"
#include "components/security_interstitials/core/pref_names.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {

void LogMixedFormInterstitialMetrics(
    security_interstitials::InsecureFormNavigationThrottle::
        InterstitialTriggeredState state) {
  base::UmaHistogramEnumeration("Security.MixedForm.InterstitialTriggerState",
                                state);
}

}  // namespace

namespace security_interstitials {

InsecureFormNavigationThrottle::InsecureFormNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory)
    : content::NavigationThrottle(navigation_handle),
      blocking_page_factory_(std::move(blocking_page_factory)) {}

InsecureFormNavigationThrottle::~InsecureFormNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
InsecureFormNavigationThrottle::WillStartRequest() {
  return GetThrottleResultForMixedForm(false /* is_redirect */);
}

content::NavigationThrottle::ThrottleCheckResult
InsecureFormNavigationThrottle::WillRedirectRequest() {
  return GetThrottleResultForMixedForm(true /* is_redirect */);
}

content::NavigationThrottle::ThrottleCheckResult
InsecureFormNavigationThrottle::WillProcessResponse() {
  // If there is an InsecureFormTabStorage associated to `web_contents_`, clear
  // the IsProceeding flag, except when prerendering.
  InsecureFormTabStorage* tab_storage = InsecureFormTabStorage::FromWebContents(
      navigation_handle()->GetWebContents());
  if (tab_storage && !navigation_handle()->IsInPrerenderedMainFrame()) {
    tab_storage->SetIsProceeding(false);
    tab_storage->SetInterstitialShown(false);
  }
  return content::NavigationThrottle::PROCEED;
}

const char* InsecureFormNavigationThrottle::GetNameForLogging() {
  return "InsecureFormNavigationThrottle";
}

// static
std::unique_ptr<InsecureFormNavigationThrottle>
InsecureFormNavigationThrottle::MaybeCreateNavigationThrottle(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    PrefService* prefs) {
  if (prefs && !prefs->GetBoolean(prefs::kMixedFormsWarningsEnabled))
    return nullptr;
  return std::make_unique<InsecureFormNavigationThrottle>(
      navigation_handle, std::move(blocking_page_factory));
}

content::NavigationThrottle::ThrottleCheckResult
InsecureFormNavigationThrottle::GetThrottleResultForMixedForm(
    bool is_redirect) {
  content::NavigationHandle* handle = navigation_handle();
  content::WebContents* contents = handle->GetWebContents();
  InsecureFormTabStorage* tab_storage =
      InsecureFormTabStorage::FromWebContents(contents);

  // We only show insecure form interstitials for form submissions. However GET
  // submissions are not marked as form submissions on reloads, so we check if
  // this navigation is coming from another mixed form interstitial.
  if (!handle->IsFormSubmission() &&
      (handle->IsInPrerenderedMainFrame() ||
       (!tab_storage || !tab_storage->InterstitialShown()))) {
    return content::NavigationThrottle::PROCEED;
  }

  // If the form is in a prerendered page, cancel it. Even though the form
  // submission wouldn't include user data (a prerender cannot provide any
  // input), the prerendered form submission could still leak data over the
  // network (e.g. the path).
  // There's an exception to this: Reloading a GET form will proceed since a
  // prerender shouldn't check the InsecureFormTabStorage, which is a per-tab
  // object. This is done in the check above.
  if (handle->IsInPrerenderedMainFrame())
    return content::NavigationThrottle::CANCEL;

  // If user has just chosen to proceed on an interstitial, we don't show
  // another one.
  if (tab_storage && tab_storage->IsProceeding())
    return content::NavigationThrottle::PROCEED;

  // Do not set special error page HTML for insecure forms in subframes; those
  // are already hard blocked.
  if (!handle->IsInOutermostMainFrame())
    return content::NavigationThrottle::PROCEED;

  url::Origin form_originating_origin =
      handle->GetInitiatorOrigin().value_or(url::Origin());
  if (!security_interstitials::IsInsecureFormActionOnSecureSource(
          form_originating_origin.GetURL(), handle->GetURL())) {
    // Currently we only warn for insecure forms in secure pages.
    return content::NavigationThrottle::PROCEED;
  }

  InterstitialTriggeredState log_state =
      InterstitialTriggeredState::kMixedFormDirect;
  bool should_proceed = false;

  if (is_redirect) {
    // 307 and 308 redirects for POST forms are special because they can leak
    // form data if done over HTTP.
    if ((handle->GetResponseHeaders()->response_code() ==
             net::HTTP_TEMPORARY_REDIRECT ||
         handle->GetResponseHeaders()->response_code() ==
             net::HTTP_PERMANENT_REDIRECT) &&
        handle->IsPost()) {
      log_state = InterstitialTriggeredState::kMixedFormRedirectWithFormData;
    } else {
      log_state = InterstitialTriggeredState::kMixedFormRedirectNoFormData;
      should_proceed = true;
    }
  }

  if (should_proceed) {
    LogMixedFormInterstitialMetrics(log_state);
    return content::NavigationThrottle::PROCEED;
  }

  LogMixedFormInterstitialMetrics(log_state);

  std::unique_ptr<InsecureFormBlockingPage> blocking_page =
      blocking_page_factory_->CreateInsecureFormBlockingPage(contents,
                                                             handle->GetURL());
  std::string interstitial_html = blocking_page->GetHTMLContents();
  SecurityInterstitialTabHelper::AssociateBlockingPage(
      handle, std::move(blocking_page));
  if (!tab_storage)
    tab_storage = InsecureFormTabStorage::GetOrCreate(contents);
  tab_storage->SetInterstitialShown(true);
  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, interstitial_html);
}

}  // namespace security_interstitials
