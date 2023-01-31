// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/federated_identity_auto_signin_permission_context_delegate.h"

namespace content {
class BrowserContext;
}

// Context for storing user permission to use the browser FedCM API's auto
// sign-in feature.
class FederatedIdentityAutoSigninPermissionContext
    : public content::FederatedIdentityAutoSigninPermissionContextDelegate,
      public KeyedService {
 public:
  explicit FederatedIdentityAutoSigninPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentityAutoSigninPermissionContext() override;

  FederatedIdentityAutoSigninPermissionContext(
      const FederatedIdentityAutoSigninPermissionContext&) = delete;
  FederatedIdentityAutoSigninPermissionContext& operator=(
      const FederatedIdentityAutoSigninPermissionContext&) = delete;

  // content::FederatedIdentityAutoSigninPermissionContextDelegate:
  bool HasAutoSigninPermission() override;

 private:
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION_CONTEXT_H_
