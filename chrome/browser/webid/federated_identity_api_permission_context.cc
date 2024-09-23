// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_api_permission_context.h"

#include "chrome/browser/browser_features.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "net/cookies/site_for_cookies.h"
#include "url/origin.h"

using PermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;

FederatedIdentityApiPermissionContext::FederatedIdentityApiPermissionContext(
    content::BrowserContext* browser_context)
    : host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(browser_context)),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context))),
      permission_autoblocker_(
          PermissionDecisionAutoBlockerFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

FederatedIdentityApiPermissionContext::
    ~FederatedIdentityApiPermissionContext() = default;

content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
FederatedIdentityApiPermissionContext::GetApiPermissionStatus(
    const url::Origin& relying_party_embedder) {
  if (!base::FeatureList::IsEnabled(features::kFedCm))
    return PermissionStatus::BLOCKED_VARIATIONS;

  const GURL rp_embedder_url = relying_party_embedder.GetURL();

  const ContentSetting setting = host_content_settings_map_->GetContentSetting(
      rp_embedder_url, rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_API);
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      break;
    case CONTENT_SETTING_BLOCK:
      return PermissionStatus::BLOCKED_SETTINGS;
    default:
      NOTREACHED_IN_MIGRATION();
      return PermissionStatus::BLOCKED_SETTINGS;
  }

  if (permission_autoblocker_->IsEmbargoed(
          rp_embedder_url, ContentSettingsType::FEDERATED_IDENTITY_API)) {
    return PermissionStatus::BLOCKED_EMBARGO;
  }

  return PermissionStatus::GRANTED;
}

void FederatedIdentityApiPermissionContext::RecordDismissAndEmbargo(
    const url::Origin& relying_party_embedder) {
  const GURL rp_embedder_url = relying_party_embedder.GetURL();
  // If content setting is allowed for `rp_embedder_url`, reset it.
  // See crbug.com/1340127 for why the resetting is not conditional on the
  // default content setting state.
  const ContentSetting setting = host_content_settings_map_->GetContentSetting(
      rp_embedder_url, rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_API);
  if (setting == CONTENT_SETTING_ALLOW) {
    host_content_settings_map_->SetContentSettingDefaultScope(
        rp_embedder_url, rp_embedder_url,
        ContentSettingsType::FEDERATED_IDENTITY_API, CONTENT_SETTING_DEFAULT);
  }
  permission_autoblocker_->RecordDismissAndEmbargo(
      rp_embedder_url, ContentSettingsType::FEDERATED_IDENTITY_API,
      false /* dismissed_prompt_was_quiet */);
}

void FederatedIdentityApiPermissionContext::RemoveEmbargoAndResetCounts(
    const url::Origin& relying_party_embedder) {
  permission_autoblocker_->RemoveEmbargoAndResetCounts(
      relying_party_embedder.GetURL(),
      ContentSettingsType::FEDERATED_IDENTITY_API);
}

bool FederatedIdentityApiPermissionContext::HasThirdPartyCookiesAccess(
    content::RenderFrameHost& host,
    const GURL& provider_url,
    const url::Origin& relying_party_embedder) const {
  return cookie_settings_->IsFullCookieAccessAllowed(
      /*request_url=*/provider_url,
      /*first_party_url=*/
      net::SiteForCookies::FromOrigin(relying_party_embedder),
      /*top_frame_origin=*/relying_party_embedder,
      host.GetCookieSettingOverrides());
}
