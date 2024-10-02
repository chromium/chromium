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
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/osauth/impl/request/password_manager_auth_request.h"
#include "chromeos/ash/components/osauth/impl/request/settings_auth_request.h"
#include "chromeos/ash/components/osauth/impl/request/webauthn_auth_request.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"

namespace chromeos::auth {

using AuthReason = std::variant<ash::InSessionAuthDialogController::Reason,
                                ash::AuthRequest::Reason>;

AuthReason ToAshReason(chromeos::auth::mojom::Reason reason) {
  switch (reason) {
    case chromeos::auth::mojom::Reason::kAccessPasswordManager:
      // In theory, execution shouldn't reach this case because this
      // implementation of the `chromeos::auth::mojom::InSessionAuth` should
      // only be reachable from ash.
      return ash::features::IsUseAuthPanelInSessionEnabled()
                 ? AuthReason{ash::AuthRequest::Reason::kPasswordManager}
                 : AuthReason{ash::InSessionAuthDialogController::
                                  kAccessPasswordManager};
    case chromeos::auth::mojom::Reason::kAccessAuthenticationSettings:
      return ash::features::IsUseAuthPanelInSessionEnabled()
                 ? AuthReason{ash::AuthRequest::Reason::kSettings}
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

std::unique_ptr<ash::AuthRequest> InSessionAuth::AuthRequestFromReason(
    ash::AuthRequest::Reason reason,
    std::u16string prompt,
    RequestTokenCallback callback) {
  switch (reason) {
    case ash::AuthRequest::Reason::kPasswordManager:
      return std::make_unique<ash::PasswordManagerAuthRequest>(
          prompt,
          base::BindOnce(&InSessionAuth::OnAuthComplete,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
    case ash::AuthRequest::Reason::kSettings:
      return std::make_unique<ash::SettingsAuthRequest>(
          base::BindOnce(&InSessionAuth::OnAuthComplete,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
    case ash::AuthRequest::Reason::kWebAuthN:
      // WebAuthN authentication requests are not made using this
      // mojo method.
      NOTREACHED();
  }
  NOTREACHED();
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
      [&](ash::AuthRequest::Reason reason) {
        ash::ActiveSessionAuthController::Get()->ShowAuthDialog(
            AuthRequestFromReason(reason,
                                  base::UTF8ToUTF16(prompt.value_or("")),
                                  std::move(callback)));
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
  if (ash::features::IsWebAuthNAuthDialogMergeEnabled()) {
    auto webauthn_auth_request =
        std::make_unique<ash::WebAuthNAuthRequest>(rp_id, std::move(callback));
    ash::ActiveSessionAuthController::Get()->ShowAuthDialog(
        std::move(webauthn_auth_request));
    return;
  }

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
