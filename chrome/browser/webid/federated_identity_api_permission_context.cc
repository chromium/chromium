// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_api_permission_context.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_result.h"
#include "content/public/common/content_features.h"
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
    const url::Origin& rp_origin) {
  if (!base::FeatureList::IsEnabled(features::kFedCm))
    return PermissionStatus::BLOCKED_VARIATIONS;

  // TODO(npm): FedCM is currently restricted to contexts where third party
  // cookies are not blocked.  Once the privacy improvements for the API are
  // implemented, remove this restriction. See https://crbug.com/13043
  if (cookie_settings_->ShouldBlockThirdPartyCookies())
    return PermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED;

  const GURL rp_url = rp_origin.GetURL();
  const ContentSetting setting = host_content_settings_map_->GetContentSetting(
      rp_url, rp_url, ContentSettingsType::FEDERATED_IDENTITY_API);
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      break;
    case CONTENT_SETTING_BLOCK:
      return PermissionStatus::BLOCKED_SETTINGS;
    default:
      NOTREACHED();
      return PermissionStatus::BLOCKED_SETTINGS;
  }

  permissions::PermissionResult embargo_result =
      permission_autoblocker_->GetEmbargoResult(
          rp_url, ContentSettingsType::FEDERATED_IDENTITY_API);
  if (embargo_result.content_setting == ContentSetting::CONTENT_SETTING_BLOCK)
    return PermissionStatus::BLOCKED_EMBARGO;
  return PermissionStatus::GRANTED;
}

void FederatedIdentityApiPermissionContext::RecordDismissAndEmbargo(
    const url::Origin& rp_origin) {
  permission_autoblocker_->RecordDismissAndEmbargo(
      rp_origin.GetURL(), ContentSettingsType::FEDERATED_IDENTITY_API,
      false /* dismissed_prompt_was_quiet */);
}

void FederatedIdentityApiPermissionContext::RemoveEmbargoAndResetCounts(
    const url::Origin& rp_origin) {
  permission_autoblocker_->RemoveEmbargoAndResetCounts(
      rp_origin.GetURL(), ContentSettingsType::FEDERATED_IDENTITY_API);
}
