// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_request_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace ambient_signin {

AmbientLoginPermissionRequest::AmbientLoginPermissionRequest(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    PermissionDecidedCallback permission_decided_callback,
    const std::u16string& username,
    const std::u16string& provider_name)
    : permissions::PermissionRequest(
          std::make_unique<permissions::PermissionRequestData>(
              permissions::RequestType::kAmbientLogin,
              /*user_gesture=*/false,
              requesting_origin,
              embedding_origin),
          std::move(permission_decided_callback),
          base::DoNothing(),
          // Disable automatic embargo so repeated dismissals of ambient sign-in
          // prompts do not trigger standard permission auto-blocking.
          /*uses_automatic_embargo=*/false),
      username_(username),
      provider_name_(provider_name) {}

AmbientLoginPermissionRequest::~AmbientLoginPermissionRequest() = default;

std::u16string AmbientLoginPermissionRequest::GetMessageTextFragment() const {
  // TODO(https://crbug.com/532206357): Ambient login uses a custom permission
  // prompt view rather than the standard "[domain] wants to <fragment>" text.
  // A non-empty string is returned here to satisfy MockPermissionPrompt in
  // unit tests until custom UI is hooked up.
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN);
}

}  // namespace ambient_signin
