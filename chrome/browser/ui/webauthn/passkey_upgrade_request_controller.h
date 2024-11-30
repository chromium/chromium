// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPGRADE_REQUEST_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPGRADE_REQUEST_CONTROLLER_H_

#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "content/public/browser/document_user_data.h"

namespace content {
class RenderFrameHost;
}

namespace device::enclave {
struct CredentialRequest;
enum class PINValidationResult;
}  // namespace device::enclave

class EnclaveManager;
class GPMEnclaveTransaction;
class Profile;

// PasskeyUpgradeRequestController is responsible for handling a request to
// silently create a passkey in GPM, effectively upgrading an existing password.
// This is also known also "conditionalCreate" in WebAuthn spec terms.
class PasskeyUpgradeRequestController
    : public content::DocumentUserData<PasskeyUpgradeRequestController>,
      public password_manager::PasswordStoreConsumer,
      public GPMEnclaveTransaction::Delegate {
 public:
  using Callback = base::OnceCallback<void(bool success)>;
  using EnclaveRequestCallback = base::RepeatingCallback<void(
      std::unique_ptr<device::enclave::CredentialRequest>)>;

  ~PasskeyUpgradeRequestController() override;

  void InitializeEnclaveRequestCallback(
      device::FidoDiscoveryFactory* discovery_factory);

  // Attempts to create a passkey for the given WebAuthn RP ID and user name, if
  // a matching password exists.
  void TryUpgradePasswordToPasskey(std::string rp_id,
                                   const std::string& user_name,
                                   Callback callback);

 private:
  enum class EnclaveState {
    kUnknown,
    kNotReady,
    kReady,
  };

  explicit PasskeyUpgradeRequestController(content::RenderFrameHost* rfh);

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // password_manager::PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      password_manager::PasswordStoreInterface* store,
      password_manager::LoginsResultOrError results_or_error) override;

  // GPMEnclaveTransaction::Delegate:
  void HandleEnclaveTransactionError() override;
  void BuildUVKeyOptions(EnclaveManager::UVKeyOptions& options) override;
  void HandlePINValidationResult(
      device::enclave::PINValidationResult result) override;
  void OnPasskeyCreated(
      const sync_pb::WebauthnCredentialSpecifics& passkey) override;

  Profile* profile() const;

  void OnEnclaveLoaded();
  void ContinuePendingUpgradeRequest();

  raw_ptr<EnclaveManager> enclave_manager_;
  EnclaveState enclave_state_ = EnclaveState::kUnknown;
  bool pending_upgrade_request_ = false;

  std::string rp_id_;
  std::u16string user_name_;
  Callback pending_callback_;

  EnclaveRequestCallback enclave_request_callback_;

  std::unique_ptr<GPMEnclaveTransaction> enclave_transaction_;

  base::WeakPtrFactory<PasskeyUpgradeRequestController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPGRADE_REQUEST_CONTROLLER_H_
