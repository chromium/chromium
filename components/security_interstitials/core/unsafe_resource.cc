// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/unsafe_resource.h"

#include "components/safe_browsing/core/browser/db/util.h"

namespace security_interstitials {

constexpr UnsafeResource::RenderProcessId UnsafeResource::kNoRenderProcessId;
constexpr UnsafeResource::FrameTreeNodeId UnsafeResource::kNoFrameTreeNodeId;

UnsafeResource::UrlCheckResult::UrlCheckResult(
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped)
    : proceed(proceed),
      showed_interstitial(showed_interstitial),
      has_post_commit_interstitial_skipped(
          has_post_commit_interstitial_skipped) {
  CHECK(!has_post_commit_interstitial_skipped || !proceed)
      << "If post commit interstitial is skipped, proceed must be false.";
}

UnsafeResource::UnsafeResource()
    : threat_type(safe_browsing::SBThreatType::SB_THREAT_TYPE_SAFE),
      is_delayed_warning(false),
      is_async_check(false) {}

UnsafeResource::UnsafeResource(const UnsafeResource& other) = default;

UnsafeResource::~UnsafeResource() = default;

bool UnsafeResource::IsMainPageLoadPendingWithSyncCheck() const {
  using enum safe_browsing::SBThreatType;
  switch (threat_type) {
    // Client-side phishing detection interstitials never block the main
    // frame load, since they happen after the page is finished loading.
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
    // Malicious ad activity reporting happens in the background.
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    // Ad sampling happens in the background.
    case SB_THREAT_TYPE_AD_SAMPLE:
    // Chrome SAVED password reuse warning happens after the page is finished
    // loading.
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    // Chrome GAIA signed in and syncing password reuse warning happens after
    // the page is finished loading.
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    // Chrome GAIA signed in and non-syncing password reuse warning happens
    // after the page is finished loading.
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    // Enterprise password reuse warning happens after the page is finished
    // loading.
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    // Suspicious site collection happens in the background
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return false;

    default:
      break;
  }

  return true;
}

void UnsafeResource::DispatchCallback(
    const base::Location& from_here,
    bool proceed,
    bool showed_interstitial,
    bool has_post_commit_interstitial_skipped) const {
  if (callback.is_null())
    return;

  DCHECK(callback_sequence);
  UrlCheckResult result(proceed, showed_interstitial,
                        has_post_commit_interstitial_skipped);
  if (callback_sequence->RunsTasksInCurrentSequence()) {
    callback.Run(result);
  } else {
    callback_sequence->PostTask(from_here, base::BindOnce(callback, result));
  }
}

}  // namespace security_interstitials
