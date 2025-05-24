// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_response_capture_navigation_throttle.h"

#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/webui_url_constants.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

using url_matcher::URLMatcher;

namespace {

constexpr char kEnrollmentFallbackUrl[] =
    "https://chromeenterprise.google/ntp-microsoft-auth";

// We consider this a common host for Microsoft authentication to be a valid
// redirection source.
constexpr char kEntraLoginHost[] = "https://login.microsoftonline.com";
// Valid redirection from MSFT Cloud App Security portal.
constexpr char kEntraMcasHost[] = "https://mcas.ms";

std::unique_ptr<URLMatcher> CreateEnrollmentRedirectUrlMatcher() {
  auto matcher = std::make_unique<URLMatcher>();
  url_matcher::util::AddAllowFiltersWithLimit(
      matcher.get(), std::vector<std::string>({kEnrollmentFallbackUrl}));
  return matcher;
}

const url_matcher::URLMatcher* GetEnrollmentRedirectUrlMatcher() {
  static base::NoDestructor<std::unique_ptr<URLMatcher>> matcher(
      CreateEnrollmentRedirectUrlMatcher());
  return matcher->get();
}

bool IsEnrollmentUrl(GURL& url) {
  return !GetEnrollmentRedirectUrlMatcher()->MatchURL(url).empty();
}

std::unique_ptr<URLMatcher> CreateMicrosoftEnrollmentUrlMatcher() {
  auto matcher = std::make_unique<URLMatcher>();
  url_matcher::util::AddAllowFiltersWithLimit(
      matcher.get(),
      std::vector<std::string>({kEntraLoginHost, kEntraMcasHost}));
  return matcher;
}

const url_matcher::URLMatcher* GetMicrosoftEnrollmentUrlMatcher() {
  static base::NoDestructor<std::unique_ptr<URLMatcher>> matcher(
      CreateMicrosoftEnrollmentUrlMatcher());
  return matcher->get();
}

}  // namespace

// static
void NtpMicrosoftAuthResponseCaptureNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& navigation_handle = registry.GetNavigationHandle();
  auto* web_contents = navigation_handle.GetWebContents();
  if (!web_contents) {
    return;
  }

  // Must be a profile using the first party NTP.
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile ||
      search::GetNewTabPageURL(profile) != chrome::kChromeUINewTabPageURL) {
    return;
  }

  // The web contents' opener or the navigation handle's parent frame
  // should be the microsoft auth iframe on the NTP.
  bool is_valid_popup_navigation =
      web_contents->HasOpener() && navigation_handle.IsInMainFrame() &&
      web_contents->GetOpener()->GetLastCommittedURL() ==
          GURL(chrome::kChromeUIUntrustedNtpMicrosoftAuthURL);
  bool is_valid_iframe_navigation =
      navigation_handle.GetParentFrame() &&
      navigation_handle.GetParentFrame()->GetLastCommittedURL() ==
          GURL(chrome::kChromeUIUntrustedNtpMicrosoftAuthURL);
  if (!is_valid_popup_navigation && !is_valid_iframe_navigation) {
    return;
  }

  registry.AddThrottle(
      std::make_unique<NtpMicrosoftAuthResponseCaptureNavigationThrottle>(
          registry));
}

NtpMicrosoftAuthResponseCaptureNavigationThrottle::
    NtpMicrosoftAuthResponseCaptureNavigationThrottle(
        content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

NtpMicrosoftAuthResponseCaptureNavigationThrottle::
    ~NtpMicrosoftAuthResponseCaptureNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
NtpMicrosoftAuthResponseCaptureNavigationThrottle::WillRedirectRequest() {
  return AttemptToTriggerInterception();
}

content::NavigationThrottle::ThrottleCheckResult
NtpMicrosoftAuthResponseCaptureNavigationThrottle::WillProcessResponse() {
  return AttemptToTriggerInterception();
}

const char*
NtpMicrosoftAuthResponseCaptureNavigationThrottle::GetNameForLogging() {
  return "NtpMicrosoftAuthResponseCaptureNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
NtpMicrosoftAuthResponseCaptureNavigationThrottle::
    AttemptToTriggerInterception() {
  if (navigation_handle()->GetRedirectChain().empty()) {
    return NtpMicrosoftAuthResponseCaptureNavigationThrottle::PROCEED;
  }

  auto url = navigation_handle()->GetURL();
  // Only try kicking off Microsoft enrollment process if a valid enroll URL is
  // seen.
  if (!IsEnrollmentUrl(url)) {
    return NtpMicrosoftAuthResponseCaptureNavigationThrottle::PROCEED;
  }

  bool accept_redirect = false;

  for (const auto& chain_url : navigation_handle()->GetRedirectChain()) {
    if (!GetMicrosoftEnrollmentUrlMatcher()->MatchURL(chain_url).empty()) {
      accept_redirect = true;
      break;
    }
  }

  if (!accept_redirect) {
    return NtpMicrosoftAuthResponseCaptureNavigationThrottle::CANCEL_AND_IGNORE;
  }

  NtpMicrosoftAuthResponseCaptureNavigationThrottle::
      RedirectNavigationHandleToAboutBlank();
  return NtpMicrosoftAuthResponseCaptureNavigationThrottle::CANCEL_AND_IGNORE;
}

// Usually Chrome forces a new |BrowsingInstance| for
// any navigations between the web and chrome-untrusted://. This would make it
// so the Microsoft Authentication Library (MSAL) integration iframe does not
// detect navigation to same origin, even if the navigation goes to the same
// URL as the iframe.
//
// This method navigates to about:blank with the URL
// fragments transferred. By navigating to about:blank and setting
// source_site_instance and initiator_origin to match the popup's opener or
// iframe's parent frame, the popup or iframe will be in the correct
// |SiteInstance| and |BrowsingInstance| to be same-origin and same-process
// with the MSAL integration's iframe.
void NtpMicrosoftAuthResponseCaptureNavigationThrottle::
    RedirectNavigationHandleToAboutBlank() {
  GURL::Replacements addRef;
  addRef.SetRefStr(navigation_handle()->GetURL().ref_piece());

  auto* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents->HasOpener() && !navigation_handle()->GetParentFrame()) {
    return;
  }
  auto* opener_or_parent_frame = navigation_handle()->GetParentFrame()
                                     ? navigation_handle()->GetParentFrame()
                                     : web_contents->GetOpener();

  // Move the URL authentication fragment into a new navigation in the opener's
  // or parent frame's |BrowsingInstance|, with the minimum number of params
  // copied over.
  content::OpenURLParams nav_params(
      GURL("about:blank").ReplaceComponents(addRef),
      content::Referrer(navigation_handle()->GetReferrer()),
      navigation_handle()->GetFrameTreeNodeId(),
      WindowOpenDisposition::CURRENT_TAB,
      navigation_handle()->GetPageTransition(),
      navigation_handle()->IsRendererInitiated());
  nav_params.source_site_instance = opener_or_parent_frame->GetSiteInstance();
  nav_params.initiator_origin =
      opener_or_parent_frame->GetLastCommittedOrigin();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](content::OpenURLParams nav_params,
                        base::WeakPtr<content::WebContents> web_contents) {
                       if (web_contents) {
                         web_contents->OpenURL(nav_params, {});
                       }
                     },
                     std::move(nav_params), web_contents->GetWeakPtr()));
}
