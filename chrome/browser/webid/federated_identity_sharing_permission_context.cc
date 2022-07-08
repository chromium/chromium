// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_sharing_permission_context.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {
const char kIdpOriginKey[] = "idp-origin";
}  // namespace

FederatedIdentitySharingPermissionContext::
    FederatedIdentitySharingPermissionContext(
        content::BrowserContext* browser_context)
    : FederatedIdentityAccountKeyedPermissionContext(
          browser_context,
          ContentSettingsType::FEDERATED_IDENTITY_SHARING,
          kIdpOriginKey) {}

FederatedIdentitySharingPermissionContext::
    ~FederatedIdentitySharingPermissionContext() = default;

bool FederatedIdentitySharingPermissionContext::HasSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return HasPermission(relying_party, relying_party, identity_provider,
                       account_id);
}

void FederatedIdentitySharingPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  GrantPermission(relying_party, relying_party, identity_provider, account_id);
}
