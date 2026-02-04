// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_referrer_throttle.h"

#include <memory>
#include <utility>

#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace dom_distiller {

// static
void DistillerReferrerThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  const auto& initiator = handle.GetInitiatorOrigin();
  content::WebContents* web_contents = handle.GetWebContents();

  // Add throttle only for navigations originating from a distiller page.
  bool is_distiller_scheme =
      (initiator && initiator->scheme() == kDomDistillerScheme);
  if (!is_distiller_scheme && web_contents) {
    is_distiller_scheme =
        web_contents->GetLastCommittedURL().SchemeIs(kDomDistillerScheme);
  }

  if (is_distiller_scheme) {
    registry.AddThrottle(std::make_unique<DistillerReferrerThrottle>(registry));
  }
}

DistillerReferrerThrottle::DistillerReferrerThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

DistillerReferrerThrottle::~DistillerReferrerThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
DistillerReferrerThrottle::WillStartRequest() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (web_contents) {
    const GURL& last_committed_url = web_contents->GetLastCommittedURL();
    if (last_committed_url.SchemeIs(kDomDistillerScheme)) {
      GURL original_url =
          url_utils::GetOriginalUrlFromDistillerUrl(last_committed_url);
      if (original_url.is_valid()) {
        auto referrer = blink::mojom::Referrer::New();
        referrer->url = original_url;
        referrer->policy =
            network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
        navigation_handle()->SetReferrer(std::move(referrer));
      }
    }
  }
  return content::NavigationThrottle::PROCEED;
}

const char* DistillerReferrerThrottle::GetNameForLogging() {
  return "DistillerReferrerThrottle";
}

}  // namespace dom_distiller
