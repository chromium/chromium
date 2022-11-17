// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "content/public/browser/back_forward_cache.h"

namespace back_forward_cache {

std::string ReasonIdToString(DisabledReasonId reason_id) {
  switch (reason_id) {
    case DisabledReasonId::kUnknown:
      return "Unknown";
    case DisabledReasonId::kPopupBlockerTabHelper:
      return "PopupBlockerTabHelper";
    case DisabledReasonId::kSafeBrowsingTriggeredPopupBlocker:
      return "SafeBrowsingTriggeredPopupBlocker";
    case DisabledReasonId::kSafeBrowsingThreatDetails:
      return "safe_browsing::ThreatDetails";
    case DisabledReasonId::kDomDistillerViewerSource:
      return "DomDistillerViewerSource";
    case DisabledReasonId::kDomDistiller_SelfDeletingRequestDelegate:
      return "browser::DomDistiller_SelfDeletingRequestDelegate";
    case DisabledReasonId::kOfflinePage:
      return "OfflinePage";
    case DisabledReasonId::kChromePasswordManagerClient_BindCredentialManager:
      return "ChromePasswordManagerClient::BindCredentialManager";
    case DisabledReasonId::kPermissionRequestManager:
      return "PermissionRequestManager";
    case DisabledReasonId::kModalDialog:
      return "ModalDialog";
    case DisabledReasonId::kExtensions:
      return "Extensions";
    case DisabledReasonId::kExtensionMessaging:
      return "ExtensionMessaging";
    case DisabledReasonId::kExtensionMessagingForOpenPort:
      return "ExtensionMessagingForOpenPort";
    case DisabledReasonId::kExtensionSentMessageToCachedFrame:
      return "ExtensionSentMessageToCachedFrame";
    case DisabledReasonId::kOomInterventionTabHelper:
      return "OomInterventionTabHelper";
  }
}

// Report string used for NotRestoredReasons API. This will be brief and will
// mask extension related reasons as "Extensions".
std::string ReasonIdToReportString(DisabledReasonId reason_id) {
  switch (reason_id) {
    case DisabledReasonId::kExtensions:
    case DisabledReasonId::kExtensionMessaging:
    case DisabledReasonId::kExtensionMessagingForOpenPort:
    case DisabledReasonId::kExtensionSentMessageToCachedFrame:
      return "Extensions";
    default:
      return ReasonIdToString(reason_id);
  }
}

content::BackForwardCache::DisabledReason DisabledReason(
    DisabledReasonId reason_id,
    const std::string& context) {
  return content::BackForwardCache::DisabledReason(
      content::BackForwardCache::DisabledSource::kEmbedder,
      static_cast<content::BackForwardCache::DisabledReasonType>(reason_id),
      ReasonIdToString(reason_id), context, ReasonIdToReportString(reason_id));
}
}  // namespace back_forward_cache
