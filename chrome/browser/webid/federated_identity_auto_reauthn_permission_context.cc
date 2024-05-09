// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "url/origin.h"

FederatedIdentityAutoReauthnPermissionContext::
    FederatedIdentityAutoReauthnPermissionContext(
        content::BrowserContext* browser_context)
    : host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(browser_context)),
      permission_autoblocker_(
          PermissionDecisionAutoBlockerFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))),
      browser_context_(browser_context) {}

FederatedIdentityAutoReauthnPermissionContext::
    ~FederatedIdentityAutoReauthnPermissionContext() = default;

bool FederatedIdentityAutoReauthnPermissionContext::
    IsAutoReauthnSettingEnabled() {
  return host_content_settings_map_->GetDefaultContentSetting(
             ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
             /*provider_id=*/nullptr) !=
             ContentSetting::CONTENT_SETTING_BLOCK &&
         PasswordManagerSettingsServiceFactory::GetForProfile(
             Profile::FromBrowserContext(browser_context_))
             ->IsSettingEnabled(
                 password_manager::PasswordManagerSetting::kAutoSignIn);
}

bool FederatedIdentityAutoReauthnPermissionContext::IsAutoReauthnEmbargoed(
    const url::Origin& relying_party_embedder) {
  return permission_autoblocker_->IsEmbargoed(
      relying_party_embedder.GetURL(),
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

base::Time
FederatedIdentityAutoReauthnPermissionContext::GetAutoReauthnEmbargoStartTime(
    const url::Origin& relying_party_embedder) {
  return permission_autoblocker_->GetEmbargoStartTime(
      relying_party_embedder.GetURL(),
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

void FederatedIdentityAutoReauthnPermissionContext::RecordEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {
  const GURL rp_embedder_url = relying_party_embedder.GetURL();
  permission_autoblocker_->RecordDisplayAndEmbargo(
      rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

void FederatedIdentityAutoReauthnPermissionContext::RemoveEmbargoForAutoReauthn(
    const url::Origin& relying_party_embedder) {
  const GURL rp_embedder_url = relying_party_embedder.GetURL();
  permission_autoblocker_->RemoveEmbargoAndResetCounts(
      rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}

void FederatedIdentityAutoReauthnPermissionContext::SetRequiresUserMediation(
    const url::Origin& rp_origin,
    bool requires_user_mediation) {
  const GURL rp_url = rp_origin.GetURL();
  host_content_settings_map_->SetContentSettingDefaultScope(
      rp_url, rp_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      requires_user_mediation ? CONTENT_SETTING_BLOCK : CONTENT_SETTING_ALLOW);
}

bool FederatedIdentityAutoReauthnPermissionContext::RequiresUserMediation(
    const url::Origin& rp_origin) {
  const GURL rp_url = rp_origin.GetURL();
  return host_content_settings_map_->GetContentSetting(
             rp_url, rp_url,
             ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION) ==
         ContentSetting::CONTENT_SETTING_BLOCK;
}
