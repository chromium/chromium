// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_signin_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/common/content_settings_types.h"

FederatedIdentityAutoSigninPermissionContext::
    FederatedIdentityAutoSigninPermissionContext(
        content::BrowserContext* browser_context)
    : host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(browser_context)) {}

FederatedIdentityAutoSigninPermissionContext::
    ~FederatedIdentityAutoSigninPermissionContext() = default;

bool FederatedIdentityAutoSigninPermissionContext::HasAutoSigninPermission() {
  return host_content_settings_map_->GetDefaultContentSetting(
             ContentSettingsType::FEDERATED_IDENTITY_AUTO_SIGNIN_PERMISSION,
             /*provider_id=*/nullptr) != ContentSetting::CONTENT_SETTING_BLOCK;
}
