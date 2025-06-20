// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/metrics/histogram_macros.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "url/origin.h"

FederatedIdentityAutoReauthnPermissionContext::
    FederatedIdentityAutoReauthnPermissionContext(
        HostContentSettingsMap* host_content_settings_map,
        permissions::PermissionDecisionAutoBlocker* permission_autoblocker)
    : host_content_settings_map_(host_content_settings_map),
      permission_autoblocker_(permission_autoblocker) {}

FederatedIdentityAutoReauthnPermissionContext::
    ~FederatedIdentityAutoReauthnPermissionContext() = default;

void FederatedIdentityAutoReauthnPermissionContext::
    OnPasswordManagerSettingsServiceInitialized(
        password_manager::PasswordManagerSettingsService* settings_service) {
  password_manager_settings_service_ = settings_service;
}

void FederatedIdentityAutoReauthnPermissionContext::Shutdown() {
  password_manager_settings_service_ = nullptr;
}

bool FederatedIdentityAutoReauthnPermissionContext::
    IsAutoReauthnSettingEnabled() {
  return password_manager_settings_service_ &&
         password_manager_settings_service_->IsSettingEnabled(
             password_manager::PasswordManagerSetting::kAutoSignIn) &&
         host_content_settings_map_->GetDefaultContentSetting(
             ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
             /*provider_id=*/nullptr) != ContentSetting::CONTENT_SETTING_BLOCK;
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
