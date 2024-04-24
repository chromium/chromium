// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACK_FORWARD_CACHE_DISABLED_REASON_ID_H_
#define COMPONENTS_BACK_FORWARD_CACHE_DISABLED_REASON_ID_H_

#include <cstdint>

namespace back_forward_cache {

// Reasons to disable BackForwardCache for this frame for chrome features.
enum class DisabledReasonId : uint16_t {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. kMaxValue is not defined because
  // this enum is not logged directly as an enum (see
  // BackForwardCache::DisabledSource).
  kUnknown = 0,
  kPopupBlockerTabHelper = 1,
  kSafeBrowsingTriggeredPopupBlocker = 2,
  kSafeBrowsingThreatDetails = 3,
  // Unblocked by https://crbug.com/1276864
  // kAppBannerManager = 4,
  kDomDistillerViewerSource = 5,
  kDomDistiller_SelfDeletingRequestDelegate = 6,
  kOomInterventionTabHelper = 7,
  kOfflinePage = 8,
  kChromePasswordManagerClient_BindCredentialManager = 9,
  kPermissionRequestManager = 10,
  // Modal dialog such as form resubmittion or http password dialog is shown for
  // the page.
  kModalDialog = 11,
  // Support for extensions is added in stages (see crbug.com/1110891), each
  // with its own enum.
  // - kExtensions: All extensions are blocklisted.
  // - kExtensionMessaging: Extensions using messaging APIs are blocklisted.
  // - kExtensionMessagingForOpenPort: Extensions using long-lived connections
  //   that don't close the connection before attempting to cache the frame are
  //   blocklisted.
  // - kExtensionSentMessageToCachedFrame: Extensions using long-lived
  //   connections that attempt to send a message to a frame while it is cached
  //   (inactive) are blocklisted.
  // kExtensions = 12. Removed
  kExtensionMessaging = 13,
  // kExtensionMessagingForOpenPort = 14. Removed
  kExtensionSentMessageToCachedFrame = 15,
  // Android WebView client requested to disable BFCache. See
  // NavigationWebMessageSender.
  kRequestedByWebViewClient = 16,
  // New reasons should be accompanied by a comment as to why BackForwardCache
  // cannot be used in this case and a link to a bug to fix that if it is
  // fixable.
  // Any updates here should be reflected in tools/metrics/histograms/enums.xml
};

}  // namespace back_forward_cache

#endif  // COMPONENTS_BACK_FORWARD_CACHE_DISABLED_REASON_ID_H_
