// Copyright 2021 The Chromium Authors. All rights reserved.
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
    case DisabledReasonId::kAppBannerManager:
      return "banners::AppBannerManager";
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
    default:
      return "Unknown (default)";
  }
}

content::BackForwardCache::DisabledReason DisabledReason(
    DisabledReasonId reason_id) {
  return content::BackForwardCache::DisabledReason(
      {content::BackForwardCache::DisabledSource::kEmbedder,
       static_cast<content::BackForwardCache::DisabledReasonType>(reason_id),
       ReasonIdToString(reason_id)});
}
}  // namespace back_forward_cache
