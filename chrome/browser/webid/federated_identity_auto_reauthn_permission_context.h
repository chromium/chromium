// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"

namespace content {
class BrowserContext;
}

namespace permissions {
class PermissionDecisionAutoBlocker;
}

namespace url {
class Origin;
}

// Context for storing user permission to use the browser FedCM API's auto
// sign-in feature.
class FederatedIdentityAutoReauthnPermissionContext
    : public content::FederatedIdentityAutoReauthnPermissionContextDelegate,
      public KeyedService {
 public:
  explicit FederatedIdentityAutoReauthnPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentityAutoReauthnPermissionContext() override;

  FederatedIdentityAutoReauthnPermissionContext(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;
  FederatedIdentityAutoReauthnPermissionContext& operator=(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;

  // content::FederatedIdentityAutoReauthnPermissionContextDelegate:
  bool IsAutoReauthnSettingEnabled() override;
  bool IsAutoReauthnEmbargoed(
      const url::Origin& relying_party_embedder) override;
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
  const raw_ptr<permissions::PermissionDecisionAutoBlocker, DanglingUntriaged>
      permission_autoblocker_;
  const raw_ptr<content::BrowserContext> browser_context_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
