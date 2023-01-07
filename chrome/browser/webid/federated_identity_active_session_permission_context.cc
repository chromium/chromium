// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_active_session_permission_context.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {
const char kIdpKey[] = "identity-provider";
}  // namespace

FederatedIdentityActiveSessionPermissionContext::
    FederatedIdentityActiveSessionPermissionContext(
        content::BrowserContext* browser_context)
    : FederatedIdentityAccountKeyedPermissionContext(
          browser_context,
          ContentSettingsType::FEDERATED_IDENTITY_ACTIVE_SESSION,
          kIdpKey) {}

FederatedIdentityActiveSessionPermissionContext::
    ~FederatedIdentityActiveSessionPermissionContext() = default;

bool FederatedIdentityActiveSessionPermissionContext::HasActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  return HasPermission(relying_party_requester, relying_party_requester,
                       identity_provider, account_identifier);
}

void FederatedIdentityActiveSessionPermissionContext::GrantActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  GrantPermission(relying_party_requester, relying_party_requester,
                  identity_provider, account_identifier);
}

void FederatedIdentityActiveSessionPermissionContext::RevokeActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  RevokePermission(relying_party_requester, relying_party_requester,
                   identity_provider, account_identifier);
}
