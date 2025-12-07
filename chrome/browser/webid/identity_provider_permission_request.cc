// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/identity_provider_permission_request.h"

#include "base/functional/callback_helpers.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"

IdentityProviderPermissionRequest::IdentityProviderPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback)
    : PermissionRequest(
          std::make_unique<permissions::PermissionRequestData>(
              std::make_unique<permissions::ContentSettingPermissionResolver>(
                  ContentSettingsType::FEDERATED_IDENTITY_API),
              /*user_gesture=*/true,
              origin.GetURL()),
          base::BindRepeating(
              &IdentityProviderPermissionRequest::PermissionDecided,
              base::Unretained(this))),
      callback_(std::move(callback)) {}

IdentityProviderPermissionRequest::~IdentityProviderPermissionRequest() =
    default;

void IdentityProviderPermissionRequest::PermissionDecided(
    PermissionDecision decision,
    bool is_final_decision,
    const permissions::PermissionRequestData& request_data) {
  DCHECK(decision != PermissionDecision::kAllowThisTime);
  DCHECK(is_final_decision);

  if (decision == PermissionDecision::kAllow) {
    std::move(callback_).Run(true);
  } else if (decision == PermissionDecision::kDeny) {
    std::move(callback_).Run(false);
  } else {
    DCHECK_EQ(PermissionDecision::kNone, decision);
    std::move(callback_).Run(false);
  }
}
