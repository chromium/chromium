// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/in_session_auth/in_session_auth.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/notreached.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace chromeos::auth {

ash::InSessionAuthDialogController::Reason ToAshReason(
    chromeos::auth::mojom::Reason reason) {
  switch (reason) {
    case chromeos::auth::mojom::Reason::kAccessPasswordManager:
      // In theory, execution shouldn't reach this case because this
      // implementation of the `chromeos::auth::mojom::InSessionAuth` should
      // only be reachable from ash.
      return ash::InSessionAuthDialogController::kAccessPasswordManager;
    case chromeos::auth::mojom::Reason::kAccessAuthenticationSettings:
      return ash::InSessionAuthDialogController::kAccessAuthenticationSettings;
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
                                 const absl::optional<std::string>& prompt,
                                 RequestTokenCallback callback) {
  ash::InSessionAuthDialogController::Get()->ShowAuthDialog(
      ToAshReason(reason),
      base::BindOnce(&InSessionAuth::OnAuthComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
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

void InSessionAuth::OnAuthComplete(RequestTokenCallback callback,
                                   bool success,
                                   const ash::AuthProofToken& token,
                                   base::TimeDelta timeout) {
  std::move(callback).Run(
      success ? chromeos::auth::mojom::RequestTokenReply::New(token, timeout)
              : nullptr);
}

}  // namespace chromeos::auth
