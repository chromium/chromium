// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/webid/federated_identity_auto_reauthn_permission_context_delegate.h"

namespace password_manager {
class PasswordManagerSettingsService;
}

namespace permissions {
class PermissionDecisionAutoBlocker;
}

namespace url {
class Origin;
}

class HostContentSettingsMap;

// Context for storing user permission to use the browser FedCM API's auto
// sign-in feature.
class FederatedIdentityAutoReauthnPermissionContext
    : public content::FederatedIdentityAutoReauthnPermissionContextDelegate,
      public KeyedService {
 public:
  // DO NOT pass a Profile here. Inject other keyed service dependencies
  // explicitly and add a corresponding DependsOn() in the factory. See
  // crbug.com/368297674.
  // `host_content_settings_map` and `permission_autoblocker` must be non-null
  // and outlive this service.
  FederatedIdentityAutoReauthnPermissionContext(
      HostContentSettingsMap* host_content_settings_map,
      permissions::PermissionDecisionAutoBlocker* permission_autoblocker);

  FederatedIdentityAutoReauthnPermissionContext(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;
  FederatedIdentityAutoReauthnPermissionContext& operator=(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;

  ~FederatedIdentityAutoReauthnPermissionContext() override;

  // Initializes cyclic dependency. `settings_service` must be non-null and
  // can be used until Shutdown().
  void OnPasswordManagerSettingsServiceInitialized(
      password_manager::PasswordManagerSettingsService* settings_service);

  // KeyedService:
  void Shutdown() override;

  // content::FederatedIdentityAutoReauthnPermissionContextDelegate:
  bool IsAutoReauthnSettingEnabled() override;
  bool IsAutoReauthnEmbargoed(
      const url::Origin& relying_party_embedder) override;
  bool IsAutoReauthnDisabledByEmbedder(
      content::WebContents* web_contents) override;
  base::Time GetAutoReauthnEmbargoStartTime(
      const url::Origin& relying_party_embedder) override;
  void RecordEmbargoForAutoReauthn(
      const url::Origin& relying_party_embedder) override;
  void RemoveEmbargoForAutoReauthn(
      const url::Origin& relying_party_embedder) override;
  void SetRequiresUserMediation(const url::Origin& rp_origin,
                                bool requires_user_mediation) override;
  bool RequiresUserMediation(const url::Origin& rp_origin) override;

 private:
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  const raw_ptr<permissions::PermissionDecisionAutoBlocker>
      permission_autoblocker_;

  raw_ptr<password_manager::PasswordManagerSettingsService>
      password_manager_settings_service_ = nullptr;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
