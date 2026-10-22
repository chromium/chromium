// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/ambient/ambient_login_permission_request.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_prompt_decision.h"
#include "components/permissions/permission_request_data.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"

namespace ambient_signin {

AmbientLoginPermissionRequest::AmbientLoginPermissionRequest(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    std::vector<PasskeyOrPasswordCredential> credentials,
    PasskeyOrPasswordSelectedCallback credential_selected_callback,
    std::vector<FederatedCredential> federated_credentials,
    FederatedCredentialSelectedCallback federated_selected_callback,
    PermissionDecidedCallback permission_decided_callback,
    bool bubble_starts_open)
    : permissions::PermissionRequest(
          std::make_unique<permissions::PermissionRequestData>(
              permissions::RequestType::kAmbientLogin,
              /*user_gesture=*/false,
              requesting_origin,
              embedding_origin),
          base::BindRepeating(
              &AmbientLoginPermissionRequest::OnPermissionDecided,
              base::Unretained(this)),
          base::DoNothing(),
          // Disable automatic embargo so repeated dismissals of ambient sign-in
          // prompts do not trigger standard permission auto-blocking.
          /*uses_automatic_embargo=*/false),
      credentials_(std::move(credentials)),
      credential_selected_callback_(std::move(credential_selected_callback)),
      federated_credentials_(std::move(federated_credentials)),
      federated_selected_callback_(std::move(federated_selected_callback)),
      caller_permission_decided_callback_(
          std::move(permission_decided_callback)),
      bubble_starts_open_(bubble_starts_open) {}

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
  selected_credential_index_ = index;
  selected_federated_index_.reset();
}

void AmbientLoginPermissionRequest::SelectFederatedCredential(size_t index) {
  CHECK_LT(index, federated_credentials_.size());
  selected_federated_index_ = index;
  selected_credential_index_.reset();
}

void AmbientLoginPermissionRequest::OnPermissionDecided(
    const permissions::PermissionPromptDecision& decision,
    const permissions::PermissionRequestData& request_data) {
  if (decision.overall_decision == ::PermissionDecision::kAllow ||
      decision.overall_decision == ::PermissionDecision::kAllowThisTime) {
    if (selected_federated_index_.has_value()) {
      credential_selected_callback_.Reset();
      if (federated_selected_callback_) {
        std::move(federated_selected_callback_).Run(*selected_federated_index_);
      }
    } else if (!credentials_.empty()) {
      federated_selected_callback_.Reset();
      if (credential_selected_callback_) {
        std::move(credential_selected_callback_)
            .Run(selected_credential_index_.value_or(0));
      }
    } else if (!federated_credentials_.empty()) {
      credential_selected_callback_.Reset();
      if (federated_selected_callback_) {
        std::move(federated_selected_callback_).Run(0);
      }
    }
  }
  if (caller_permission_decided_callback_) {
    caller_permission_decided_callback_.Run(decision, request_data);
  }
}

// static
const gfx::VectorIcon& AmbientLoginPermissionRequest::GetCredentialIcon(
    CredentialType type) {
  switch (type) {
    case CredentialType::kPasskey:
      return features::IsRoundedIconsEnabled() ? vector_icons::kPasskeyIcon
                                               : vector_icons::kPasskeyOldIcon;
    case CredentialType::kPassword:
      return features::IsRoundedIconsEnabled()
                 ? vector_icons::kPasswordManagerIcon
                 : vector_icons::kPasswordManagerOldIcon;
  }
}

permissions::IconId AmbientLoginPermissionRequest::GetIconForChip() const {
  if (!credentials_.empty()) {
    return GetCredentialIcon(credentials_.front().type);
  }
  // This matches the bubble's icon for a federated credential without an
  // account picture.
  // TODO(crbug.com/532206357): When the top federated credential has an
  // account picture, the bubble shows that picture. Showing it in the chip as
  // well needs `ui::ImageModel` support in the permission chip, which only
  // accepts vector icons today.
  return GooglePasswordManagerVectorIcon();
}

permissions::IconId AmbientLoginPermissionRequest::GetBlockedIconForChip()
    const {
  // Ambient sign-in has no blocked state. If the chip collapses, it keeps
  // showing the credential icon.
  return GetIconForChip();
}

bool AmbientLoginPermissionRequest::ShouldKeepChipOnBubbleClose() const {
  // Closing the bubble only hides the credentials, so that the user can still
  // sign in from the chip.
  return true;
}

std::optional<std::u16string> AmbientLoginPermissionRequest::GetRequestChipText(
    ChipTextType type) const {
  // If the chosen credential signs the user in without further UI, the chip
  // shows that the user is signing in, rather than going away.
  // TODO(crbug.com/567563304): End the chip when sign-in finishes or fails,
  // rather than after the fixed time that the chip shows confirmations for.
  if ((type == ChipTextType::ALLOW_CONFIRMATION ||
       type == ChipTextType::ACCESSIBILITY_ALLOWED_CONFIRMATION) &&
      selected_federated_index_ &&
      federated_credentials_[*selected_federated_index_].signs_in_directly) {
    return l10n_util::GetStringUTF16(IDS_FEDERATION_SIGNING_IN_TITLE);
  }
  return permissions::PermissionRequest::GetRequestChipText(type);
}

std::u16string AmbientLoginPermissionRequest::GetMessageTextFragment() const {
  // TODO(https://crbug.com/532206357): Ambient login uses a custom permission
  // prompt view rather than the standard "[domain] wants to <fragment>" text.
  // A non-empty string is returned here to satisfy MockPermissionPrompt in
  // unit tests until custom UI is hooked up.
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN);
}

bool AmbientLoginPermissionRequest::ShouldBubbleStartOpen() const {
  return bubble_starts_open_;
}

}  // namespace ambient_signin
