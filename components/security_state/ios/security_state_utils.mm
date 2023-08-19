// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/ios/security_state_utils.h"

#include <memory>

#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/security_state/core/security_state.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/security_style.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#include "url/origin.h"

namespace security_state {

MaliciousContentStatus GetMaliciousContentStatus(
    const web::WebState* web_state) {
  // There is no known malicious content if there is no allow list or no visible
  // item.
  const SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(web_state);
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!allow_list || !visible_item)
    return security_state::MALICIOUS_CONTENT_STATUS_NONE;

  // There is no malicious content if there is no allowed unsafe resource and no
  // pending decision.
  const GURL& visible_url = visible_item->GetURL();
  std::set<safe_browsing::SBThreatType> threats;
  bool is_unsafe_resource_allowed_or_pending =
      allow_list->AreUnsafeNavigationsAllowed(visible_url, &threats) ||
      allow_list->IsUnsafeNavigationDecisionPending(visible_url, &threats);
  if (!is_unsafe_resource_allowed_or_pending)
    return security_state::MALICIOUS_CONTENT_STATUS_NONE;

  // Return the appropriate MaliciousContentStatus from the allowed or pending
  // threat type.
  DCHECK(!threats.empty());
  switch (*threats.begin()) {
    case safe_browsing::SB_THREAT_TYPE_UNUSED:
    case safe_browsing::SB_THREAT_TYPE_SAFE:
    case safe_browsing::SB_THREAT_TYPE_URL_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return security_state::MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING;
    case safe_browsing::SB_THREAT_TYPE_URL_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
    case safe_browsing::SB_THREAT_TYPE_URL_UNWANTED:
      return security_state::MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE;
    case safe_browsing::SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
    case safe_browsing::SB_THREAT_TYPE_BILLING:
      return security_state::MALICIOUS_CONTENT_STATUS_BILLING;
    case safe_browsing::
        DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
    case safe_browsing::SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case safe_browsing::SB_THREAT_TYPE_EXTENSION:
    case safe_browsing::SB_THREAT_TYPE_BLOCKLISTED_RESOURCE:
    case safe_browsing::SB_THREAT_TYPE_API_ABUSE:
    case safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case safe_browsing::SB_THREAT_TYPE_CSD_ALLOWLIST:
    case safe_browsing::SB_THREAT_TYPE_AD_SAMPLE:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_POPUP:
    case safe_browsing::SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
    case safe_browsing::SB_THREAT_TYPE_SUSPICIOUS_SITE:
    case safe_browsing::SB_THREAT_TYPE_APK_DOWNLOAD:
    case safe_browsing::SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case safe_browsing::SB_THREAT_TYPE_MANAGED_POLICY_WARN:
    case safe_browsing::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK:
      // These threat types are not currently associated with
      // interstitials, and thus resources with these threat types are
      // not ever whitelisted or pending whitelisting.
      NOTREACHED();
      break;
  }
  return security_state::MALICIOUS_CONTENT_STATUS_NONE;
}

std::unique_ptr<security_state::VisibleSecurityState>
GetVisibleSecurityStateForWebState(const web::WebState* web_state) {
  auto state = std::make_unique<security_state::VisibleSecurityState>();

  state->malicious_content_status = GetMaliciousContentStatus(web_state);

  const web::NavigationItem* item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!item || item->GetSSL().security_style == web::SECURITY_STYLE_UNKNOWN)
    return state;

  state->connection_info_initialized = true;
  state->url = item->GetURL();
  const web::SSLStatus& ssl = item->GetSSL();
  state->certificate = ssl.certificate;
  state->cert_status = ssl.cert_status;
  state->displayed_mixed_content =
      (ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT) ? true
                                                                        : false;

  return state;
}

security_state::SecurityLevel GetSecurityLevelForWebState(
    const web::WebState* web_state) {
  if (!web_state) {
    return security_state::NONE;
  }
  return security_state::GetSecurityLevel(
      *GetVisibleSecurityStateForWebState(web_state),
      false /* used policy installed certificate */);
}

}  // namespace security_state
