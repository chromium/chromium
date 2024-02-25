// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/federated_identity_api_permission_context_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}

namespace permissions {
class PermissionDecisionAutoBlocker;
}

namespace url {
class Origin;
}

// Context for storing user permission to use the browser FedCM API.
class FederatedIdentityApiPermissionContext
    : public content::FederatedIdentityApiPermissionContextDelegate,
      public KeyedService {
 public:
  explicit FederatedIdentityApiPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentityApiPermissionContext() override;

  FederatedIdentityApiPermissionContext(
      const FederatedIdentityApiPermissionContext&) = delete;
  FederatedIdentityApiPermissionContext& operator=(
      const FederatedIdentityApiPermissionContext&) = delete;

  // content::FederatedIdentityApiPermissionContextDelegate:
  content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
  GetApiPermissionStatus(const url::Origin& relying_party_embedder) override;
  void RecordDismissAndEmbargo(
      const url::Origin& relying_party_embedder) override;
  void RemoveEmbargoAndResetCounts(
      const url::Origin& relying_party_embedder) override;

  bool HasThirdPartyCookiesAccess(
      content::RenderFrameHost& host,
      const GURL& provider_url,
      const url::Origin& relying_party_embedder) const override;

 private:
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  const raw_ptr<permissions::PermissionDecisionAutoBlocker, DanglingUntriaged>
      permission_autoblocker_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_API_PERMISSION_CONTEXT_H_
