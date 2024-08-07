// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/in_session_auth/in_session_auth.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace chromeos::auth {

using AuthReason = std::variant<ash::InSessionAuthDialogController::Reason,
                                ash::ActiveSessionAuthController::Reason>;

AuthReason ToAshReason(chromeos::auth::mojom::Reason reason) {
  switch (reason) {
    case chromeos::auth::mojom::Reason::kAccessPasswordManager:
      // In theory, execution shouldn't reach this case because this
      // implementation of the `chromeos::auth::mojom::InSessionAuth` should
      // only be reachable from ash.
      return ash::features::IsUseAuthPanelInSessionEnabled()
                 ? AuthReason{ash::ActiveSessionAuthController::Reason::
                                  kPasswordManager}
                 : AuthReason{ash::InSessionAuthDialogController::
                                  kAccessPasswordManager};
    case chromeos::auth::mojom::Reason::kAccessAuthenticationSettings:
      return ash::features::IsUseAuthPanelInSessionEnabled()
                 ? AuthReason{ash::ActiveSessionAuthController::Reason::
                                  kSettings}
                 : AuthReason{ash::InSessionAuthDialogController::
                                  kAccessAuthenticationSettings};
    case chromeos::auth::mojom::Reason::kAccessMultideviceSettings:
      return ash::InSessionAuthDialogController::kAccessMultideviceSettings;
  }
}

InSessionAuth::InSessionAuth() {}

InSessionAuth::~InSessionAuth() = default;

void InSessionAuth::BindReceiver(
    mojo::PendingReceiver<chromeos::auth::mojom::InSessionAuth> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void InSessionAuth::RequestToken(chromeos::auth::mojom::Reason reason,
                                 const std::optional<std::string>& prompt,
                                 RequestTokenCallback callback) {
  auto visitor = base::Overloaded(
      // Legacy code path
      [&](ash::InSessionAuthDialogController::Reason reason) {
        ash::InSessionAuthDialogController::Get()->ShowAuthDialog(
            reason, prompt,
            base::BindOnce(&InSessionAuth::OnAuthComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
      },

      // New Code path
      [&](ash::ActiveSessionAuthController::Reason reason) {
        ash::ActiveSessionAuthController::Get()->ShowAuthDialog(
            reason,
            base::BindOnce(&InSessionAuth::OnAuthComplete,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
      });

  std::visit(visitor, ToAshReason(reason));
}

void InSessionAuth::CheckToken(chromeos::auth::mojom::Reason reason,
                               const std::string& token,
                               CheckTokenCallback callback) {
  bool token_valid;
  token_valid = ash::AuthSessionStorage::Get()->IsValid(token);
  std::move(callback).Run(token_valid);
}

void InSessionAuth::InvalidateToken(const std::string& token) {
  ash::AuthSessionStorage::Get()->Invalidate(token, base::DoNothing());
}

void InSessionAuth::RequestLegacyWebAuthn(
    const std::string& rp_id,
    const std::string& window_id,
    RequestLegacyWebAuthnCallback callback) {
  ash::InSessionAuthDialogController::Get()->ShowLegacyWebAuthnDialog(
      rp_id, window_id, std::move(callback));
}

void InSessionAuth::OnAuthComplete(RequestTokenCallback callback,
                                   bool success,
                                   const ash::AuthProofToken& token,
                                   base::TimeDelta timeout) {
  std::move(callback).Run(
      success ? chromeos::auth::mojom::RequestTokenReply::New(token, timeout)
              : nullptr);
}

}  // namespace chromeos::auth
