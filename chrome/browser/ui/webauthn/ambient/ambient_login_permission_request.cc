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
    std::vector<PasskeyOrPasswordCredential> credentials,
    PasskeyOrPasswordSelectedCallback credential_selected_callback,
    std::vector<FederatedCredential> federated_credentials,
    FederatedCredentialSelectedCallback federated_selected_callback,
    PermissionDecidedCallback permission_decided_callback)
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
      credentials_(std::move(credentials)),
      credential_selected_callback_(std::move(credential_selected_callback)),
      federated_credentials_(std::move(federated_credentials)),
      federated_selected_callback_(std::move(federated_selected_callback)) {}

AmbientLoginPermissionRequest::AmbientLoginPermissionRequest(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    std::vector<PasskeyOrPasswordCredential> credentials,
    PasskeyOrPasswordSelectedCallback credential_selected_callback,
    PermissionDecidedCallback permission_decided_callback)
    : AmbientLoginPermissionRequest(
          requesting_origin,
          embedding_origin,
          std::move(credentials),
          std::move(credential_selected_callback),
          /*federated_credentials=*/{},
          /*federated_selected_callback=*/base::NullCallback(),
          std::move(permission_decided_callback)) {}

AmbientLoginPermissionRequest::~AmbientLoginPermissionRequest() = default;

void AmbientLoginPermissionRequest::SelectCredential(size_t index) {
  CHECK_LT(index, credentials_.size());
  if (credential_selected_callback_) {
    std::move(credential_selected_callback_).Run(index);
  }
}

void AmbientLoginPermissionRequest::SelectFederatedCredential(size_t index) {
  CHECK_LT(index, federated_credentials_.size());
  if (federated_selected_callback_) {
    std::move(federated_selected_callback_).Run(index);
  }
}

std::u16string AmbientLoginPermissionRequest::GetMessageTextFragment() const {
  // TODO(https://crbug.com/532206357): Ambient login uses a custom permission
  // prompt view rather than the standard "[domain] wants to <fragment>" text.
  // A non-empty string is returned here to satisfy MockPermissionPrompt in
  // unit tests until custom UI is hooked up.
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN);
}

}  // namespace ambient_signin
