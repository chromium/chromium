// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "url/origin.h"

FederatedIdentityAutoReauthnPermissionContext::
    FederatedIdentityAutoReauthnPermissionContext(
        content::BrowserContext* browser_context)
    : host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(browser_context)),
      permission_autoblocker_(
          PermissionDecisionAutoBlockerFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

FederatedIdentityAutoReauthnPermissionContext::
    ~FederatedIdentityAutoReauthnPermissionContext() = default;

bool FederatedIdentityAutoReauthnPermissionContext::HasAutoReauthnPermission(
    const url::Origin& relying_party_embedder) {
  bool is_content_setting_allowed =
      host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
          /*provider_id=*/nullptr) != ContentSetting::CONTENT_SETTING_BLOCK;
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.AutoReauthn.BlockedByContentSettings",
                        !is_content_setting_allowed);
  bool is_embargoed = permission_autoblocker_->IsEmbargoed(
      relying_party_embedder.GetURL(),
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
  UMA_HISTOGRAM_BOOLEAN("Blink.FedCm.AutoReauthn.BlockedByEmbargo",
                        is_embargoed);
  return is_content_setting_allowed && !is_embargoed;
}

void FederatedIdentityAutoReauthnPermissionContext::RecordDisplayAndEmbargo(
    const url::Origin& relying_party_embedder) {
  const GURL rp_embedder_url = relying_party_embedder.GetURL();
  host_content_settings_map_->SetContentSettingDefaultScope(
      rp_embedder_url, rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION,
      CONTENT_SETTING_BLOCK);
  permission_autoblocker_->RecordDisplayAndEmbargo(
      rp_embedder_url,
      ContentSettingsType::FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION);
}
