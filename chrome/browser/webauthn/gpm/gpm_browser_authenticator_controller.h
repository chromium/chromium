// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_GPM_BROWSER_AUTHENTICATOR_CONTROLLER_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_GPM_BROWSER_AUTHENTICATOR_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/gpm/gpm_browser_authenticator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/browser/global_routing_id.h"
#include "google_apis/gaia/gaia_id.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace device {
enum class FidoRequestType : uint8_t;
enum class UserVerificationRequirement;
}  // namespace device

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

enum class EnclaveEnabledStatus;
class Profile;

// Manages UI and key retrieval for `GpmBrowserAuthenticator`.
class GpmBrowserAuthenticatorController
    : public AuthenticatorRequestDialogModel::Observer,
      public trusted_vault::TrustedVaultClient::Observer {
 public:
  using RequestCallback =
      GpmBrowserAuthenticatorDiscovery::AddAuthenticatorCallback;

  // State of the primary account's passkeys security domain for this request.
  enum class AccountState {
    // GPM is unavailable (e.g. no credentials for get(), persistent sign-in
    // error, or security domain fetch error).
    kNone,
    // Fetching local keys or downloading security domain registration state.
    kLoading,
    // Local keys are missing, but the security domain can be recovered.
    kRecoverable,
    // Local keys are missing and recovery factors are unusable; reset required.
    kIrrecoverable,
    // The security domain has not been initialized yet.
    kEmpty,
    // Local security domain keys are available and ready for use.
    kReady,
  };

  // Coarse summary of `AccountState`.
  enum class AccountReadyState {
    kNotReady,
    kLoading,
    kReady,
  };

  GpmBrowserAuthenticatorController(
      content::RenderFrameHost& render_frame_host,
      AuthenticatorRequestDialogModel& model,
      const std::string& rp_id,
      device::FidoRequestType request_type,
      device::UserVerificationRequirement user_verification_requirement,
      RequestCallback request_callback);
  GpmBrowserAuthenticatorController(const GpmBrowserAuthenticatorController&) =
      delete;
  GpmBrowserAuthenticatorController& operator=(
      const GpmBrowserAuthenticatorController&) = delete;
  ~GpmBrowserAuthenticatorController() override;

  std::optional<EnclaveUserVerificationMethod> GetUserVerificationMethod()
      const;

  bool is_active() const;
  bool ready_for_ui() const;

  const std::vector<sync_pb::WebauthnCredentialSpecifics>& creds() const;

  AccountState account_state_for_testing() const { return account_state_; }

  AccountReadyState account_ready_state() const;
  void RunWhenAccountReady(base::OnceClosure callback);

  // Transitions the dialog to `Step::kGPMError` when the authenticator fails.
  bool OnAuthenticatorError();

  // AuthenticatorRequestDialogModel::Observer:
  void OnGPMCreationSelected() override;
  void OnGPMPasskeySelected(std::vector<uint8_t> credential_id) override;
  void OnGPMTrustThisComputer() override;
  void OnGPMCreationConfirmed() override;
  void OnGPMPasskeysReset(bool success) override;

  // trusted_vault::TrustedVaultClient::Observer:
  void OnTrustedVaultKeysChanged(
      std::optional<trusted_vault::TrustedVaultUserActionTriggerForUMA> trigger)
      override;
  void OnTrustedVaultRecoverabilityChanged() override;

 private:
  Profile* GetProfile() const;
  content::WebContents* web_contents() const;

  void OnKeysFetched(CoreAccountInfo account,
                     const std::vector<std::vector<uint8_t>>& keys);
  void DownloadAccountState(const CoreAccountInfo& account);
  void OnAccountStateDownloaded(
      std::unique_ptr<trusted_vault::TrustedVaultConnection> connection,
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result);
  void OnKeysFetchedAfterRecovery(
      const std::vector<std::vector<uint8_t>>& keys);

  void SetActive(EnclaveEnabledStatus status);
  void SetAccountState(AccountState account_state);
  void OnGpmSelectedWhileLoading();
  void OnLoadingTimeout();
  void StartTransaction();
  void OnPasskeyCreated(sync_pb::WebauthnCredentialSpecifics passkey);

  const content::GlobalRenderFrameHostId render_frame_host_id_;
  const std::string rp_id_;
  const device::FidoRequestType request_type_;
  const raw_ref<AuthenticatorRequestDialogModel> model_;
  raw_ptr<trusted_vault::TrustedVaultClient> trusted_vault_client_;

  base::ScopedObservation<AuthenticatorRequestDialogModel,
                          AuthenticatorRequestDialogModel::Observer>
      model_observer_{this};
  base::ScopedObservation<trusted_vault::TrustedVaultClient,
                          trusted_vault::TrustedVaultClient::Observer>
      trusted_vault_client_observer_{this};

  AccountState account_state_ = AccountState::kLoading;
  std::vector<sync_pb::WebauthnCredentialSpecifics> creds_;
  std::vector<std::vector<uint8_t>> trusted_vault_keys_;
  std::optional<int32_t> key_version_;
  std::optional<bool> is_active_;
  bool ready_for_ui_ = false;
  bool transaction_started_ = false;
  std::optional<std::vector<uint8_t>> selected_cred_id_;

  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;
  RequestCallback request_callback_;
  base::OnceClosure waiting_for_account_state_;
  base::OneShotTimer loading_timeout_;

  base::WeakPtrFactory<GpmBrowserAuthenticatorController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_GPM_BROWSER_AUTHENTICATOR_CONTROLLER_H_
