// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPGRADE_REQUEST_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPGRADE_REQUEST_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(PasskeyUpgradeResult)
enum class PasskeyUpgradeResult {
  kSuccess = 0,
  kGpmDisabled = 1,
  kOptOut = 2,
  kEnclaveNotInitialized = 3,
  kPasswordStoreError = 4,
  kNoMatchingPassword = 5,
  kNoRecentlyUsedPassword = 6,
  kEnclaveError = 7,
  kMaxValue = kEnclaveError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:PasskeyUpgradeResult)

// Record a UMA histogram for the outcome of a passkey upgrade request.
void RecordPasskeyUpgradeResultHistogram(PasskeyUpgradeResult);

// PasskeyUpgradeRequestController is responsible for handling a request to
// silently create a passkey in GPM, effectively upgrading an existing password.
// This is also known as conditionalCreate in WebAuthn.
class PasskeyUpgradeRequestController
    : public password_manager::PasswordStoreConsumer,
      public GPMEnclaveTransaction::Delegate {
 public:
  using Callback = base::OnceCallback<void(bool success)>;
  using EnclaveRequestCallback = base::RepeatingCallback<void(
      std::unique_ptr<device::enclave::CredentialRequest>)>;

  // The Delegate interface lets the owner of the request track its outcome.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void PasskeyUpgradeSucceeded() = 0;
    virtual void PasskeyUpgradeFailed() = 0;
  };

  explicit PasskeyUpgradeRequestController(
      content::RenderFrameHost* rfh,
      EnclaveRequestCallback enclave_request_callback);

  ~PasskeyUpgradeRequestController() override;

  // Attempts to create a passkey for the given WebAuthn RP ID and user name, if
  // a matching password exists. `delegate` must be non-null and outlive `this`.
  void TryUpgradePasswordToPasskey(std::string rp_id,
                                   const std::string& username,
                                   Delegate* delegate);

 private:
  enum class EnclaveState {
    kUnknown,
    kReady,
    kError,
  };

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
  EnclaveUserVerificationMethod GetUvMethod() override;

  content::RenderFrameHost& render_frame_host() const;
  Profile* profile() const;

  void OnEnclaveLoaded();
  void ContinuePendingUpgradeRequest();
  void FinishRequest(PasskeyUpgradeResult error);

  const content::GlobalRenderFrameHostId frame_host_id_;

  const raw_ptr<EnclaveManager> enclave_manager_;
  EnclaveState enclave_state_ = EnclaveState::kUnknown;
  bool pending_request_ = false;

  std::string rp_id_;
  std::u16string username_;
  raw_ptr<Delegate> delegate_;

  EnclaveRequestCallback enclave_request_callback_;

  std::unique_ptr<GPMEnclaveTransaction> enclave_transaction_;

  base::WeakPtrFactory<PasskeyUpgradeRequestController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_PASSKEY_UPGRADE_REQUEST_CONTROLLER_H_
