// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_DIALOG_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chromeos/passkey_service.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"

namespace webauthn {
class PasskeyModel;
}

namespace chromeos {

// PasskeyDialogController makes updates to the WebAuthn tab-modal dialog for
// passkeys on ChromeOS.
class PasskeyDialogController
    : public AuthenticatorRequestDialogModel::Observer,
      public PasskeyService::Observer {
 public:
  // `dialog_model`, `passkey_service` and `passkey_model` must not be null and
  // outlive this instance.
  PasskeyDialogController(
      AuthenticatorRequestDialogModel* dialog_model,
      PasskeyService* passkey_service,
      webauthn::PasskeyModel* passkey_model,
      const std::string& rp_id,
      device::FidoRequestType request_type,
      device::UserVerificationRequirement user_verification_requirement);
  ~PasskeyDialogController() override;

  const std::vector<sync_pb::WebauthnCredentialSpecifics>& credentials() const;
  bool ready_for_ui() const;

  PasskeyService::AccountState account_state_for_testing() const;

 private:
  void OnFetchAccountState(PasskeyService::AccountState state);
  void StartAuthenticatorRequest();

  // AuthenticatorRequestDialogModel::Observer:
  void OnGPMSelected() override;
  void OnGPMPasskeySelected(std::vector<uint8_t> credential_id) override;

  // PasskeyService::Observer:
  void OnHavePasskeysDomainSecret() override;

  // This is a `KeyedService` for the current profile and outlives this object.
  const raw_ptr<PasskeyService> passkey_service_;

  // Owned by ChromeAuthenticatorRequestDelegate, which also owns this object.
  const raw_ptr<AuthenticatorRequestDialogModel> dialog_model_;

  std::optional<PasskeyService::AccountState> account_state_;

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const std::string rp_id_;

  std::vector<sync_pb::WebauthnCredentialSpecifics> credentials_;
  const device::FidoRequestType request_type_;
  const device::UserVerificationRequirement user_verification_requirement_;

  base::WeakPtrFactory<PasskeyDialogController> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_WEBAUTHN_CHROMEOS_PASSKEY_DIALOG_CONTROLLER_H_
