// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/identity_provider_permission_request.h"

IdentityProviderPermissionRequest::IdentityProviderPermissionRequest(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback)
    : PermissionRequest(
          origin.GetURL(),
          permissions::RequestType::kIdentityProvider,
          /*has_gesture=*/true,
          base::BindRepeating(
              &IdentityProviderPermissionRequest::PermissionDecided,
              base::Unretained(this)),
          base::BindOnce(&IdentityProviderPermissionRequest::DeleteRequest,
                         base::Unretained(this))),
      callback_(std::move(callback)) {}

IdentityProviderPermissionRequest::~IdentityProviderPermissionRequest() =
    default;

void IdentityProviderPermissionRequest::PermissionDecided(
    ContentSetting result,
    bool is_one_time,
    bool is_final_decision) {
  DCHECK(!is_one_time);
  DCHECK(is_final_decision);

  if (result == ContentSetting::CONTENT_SETTING_ALLOW) {
    std::move(callback_).Run(true);
  } else if (result == ContentSetting::CONTENT_SETTING_BLOCK) {
    std::move(callback_).Run(false);
  } else {
    DCHECK_EQ(CONTENT_SETTING_DEFAULT, result);
    std::move(callback_).Run(false);
  }
}

void IdentityProviderPermissionRequest::DeleteRequest() {
  delete this;
}
