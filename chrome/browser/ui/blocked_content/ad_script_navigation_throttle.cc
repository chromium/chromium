// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/ad_script_navigation_throttle.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/constants.h"
#endif

namespace {

// Returns true if the navigation is renderer-initiated by an ad script
// without user activation and targets a URL cross-site from the page.
bool IsAdNavigationWithoutUserGesture(
    content::NavigationHandle& navigation_handle) {
  CHECK(navigation_handle.IsInPrimaryMainFrame());
  CHECK(!navigation_handle.HasCommitted());

  if (!navigation_handle.IsRendererInitiated() ||
      !navigation_handle.StartedByAd() ||
      navigation_handle.StartedWithTransientActivation()) {
    return false;
  }

  // `GetOriginToCommit()` returns `std::nullopt` if the response will not
  // commit a document (e.g., downloads or HTTP 204/205 responses).
  std::optional<url::Origin> origin_to_commit =
      navigation_handle.GetOriginToCommit();
  if (!origin_to_commit.has_value()) {
    return false;
  }

  // If this tab is still on its initial empty document (e.g., opened via
  // `w = window.open("")` with a user gesture — which is gated by the popup
  // blocker and consumes the user gesture — and then navigated via
  // `w.location = ...`), allow the navigation to proceed.
  if (navigation_handle.IsNavigatingFromInitialEmptyDocument()) {
    return false;
  }

  content::WebContents* contents = navigation_handle.GetWebContents();
  // Same-site navigations with the last committed origin are exempt.
  if (net::SchemefulSite::IsSameSite(
          contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
          *origin_to_commit)) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Exempt navigating to or from extension URLs.
  if (navigation_handle.GetURL().SchemeIs(extensions::kExtensionScheme) ||
      contents->GetLastCommittedURL().SchemeIs(extensions::kExtensionScheme)) {
    return false;
  }
#endif

  return true;
}

}  // namespace

void AdScriptNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (registry.GetNavigationHandle().IsInPrimaryMainFrame()) {
    registry.AddThrottle(
        std::make_unique<AdScriptNavigationThrottle>(registry));
  }
}

AdScriptNavigationThrottle::AdScriptNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

AdScriptNavigationThrottle::~AdScriptNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
AdScriptNavigationThrottle::WillProcessResponse() {
  if (!IsAdNavigationWithoutUserGesture(*navigation_handle())) {
    return content::NavigationThrottle::PROCEED;
  }

  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      navigation_handle()->GetWebContents()->GetPrimaryMainFrame(),
      blink::mojom::WebFeature::kAdScriptMainFrameNavigationWithoutUserGesture);

  return content::NavigationThrottle::PROCEED;
}

const char* AdScriptNavigationThrottle::GetNameForLogging() {
  return "AdScriptNavigationThrottle";
}
