// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACK_FORWARD_CACHE_BACK_FORWARD_CACHE_DISABLE_H_
#define COMPONENTS_BACK_FORWARD_CACHE_BACK_FORWARD_CACHE_DISABLE_H_

#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/global_routing_id.h"

namespace back_forward_cache {
// Reasons to disable BackForwardCache for this frame for chrome features.
enum class DisabledReasonId : content::BackForwardCache::DisabledReasonType {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. kMaxValue is not defined because
  // this enum is not logged directly as an enum (see
  // BackForwardCache::DisabledSource).
  kUnknown = 0,
  kPopupBlockerTabHelper = 1,
  kSafeBrowsingTriggeredPopupBlocker = 2,
  kSafeBrowsingThreatDetails = 3,
  kAppBannerManager = 4,
  kDomDistillerViewerSource = 5,
  kDomDistiller_SelfDeletingRequestDelegate = 6,
  kOomInterventionTabHelper = 7,
  kOfflinePage = 8,
  kChromePasswordManagerClient_BindCredentialManager = 9,
  kPermissionRequestManager = 10,
  // Modal dialog such as form resubmittion or http password dialog is shown for
  // the page.
  kModalDialog = 11,
  kExtensions = 12,
  kExtensionMessaging = 13,
  // New reasons should be accompanied by a comment as to why BackForwardCache
  // cannot be used in this case and a link to a bug to fix that if it is
  // fixable.
  // Any updates here should be reflected in tools/metrics/histograms/enums.xml
};

// Constructs a chrome-specific DisabledReason
content::BackForwardCache::DisabledReason DisabledReason(
    DisabledReasonId reason_id);
}  // namespace back_forward_cache

#endif  // COMPONENTS_BACK_FORWARD_CACHE_BACK_FORWARD_CACHE_DISABLE_H_
