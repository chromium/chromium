// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_TRANSACTION_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_TRANSACTION_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/webauthn/enclave_manager.h"

namespace device {
enum class FidoRequestType : uint8_t;
}

namespace device::enclave {
struct ClaimedPIN;
struct CredentialRequest;
enum class PINValidationResult : int;
}  // namespace device::enclave

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

namespace webauthn {
class PasskeyModel;
}

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

// EnclaveUserVerificationMethod enumerates the possible ways that user
// verification will be performed for an enclave transaction.
enum class EnclaveUserVerificationMethod {
  // No user verification will be performed.
  kNone,
  // The user will enter a GPM PIN.
  kPIN,
  // User verification is satisfied because the user performed account recovery.
  kImplicit,
  // The operating system will perform user verification and allow signing
  // with the UV key.
  kUVKeyWithSystemUI,
  // The device is in a state waiting for an OS UV key to be created, which can
  // be done when a UV request is required.
  kDeferredUVKeyWithSystemUI,
  // Chrome will show user verification UI for the operating system, which will
  // then allow signing
  // with the UV key.
  kUVKeyWithChromeUI,
  // The request cannot be satisfied.
  kUnsatisfiable,
};

// GPMEnclaveTransaction encapsulates a single request (MakeCredential or
// GetAssertion) made to the enclave authenticator.
class GPMEnclaveTransaction {
 public:
  // Dispatches a CredentialRequest instance to the EnclaveAuthenticator.
  using EnclaveRequestCallback = base::RepeatingCallback<void(
      std::unique_ptr<device::enclave::CredentialRequest>)>;

  class Delegate {
   public:
    // Invoked when transaction encounters an error. This is useful for updating
    // UI.
    virtual void HandleEnclaveTransactionError() = 0;

    // Invoked to build UVKeyOptions for the request. This is only invoked for
    // requests that perform UV.
    virtual void BuildUVKeyOptions(EnclaveManager::UVKeyOptions& options) = 0;

    // Invoked with the result of the PIN claim of the transaction if PIN UV was
    // performed.
    virtual void HandlePINValidationResult(
        device::enclave::PINValidationResult result) = 0;

    // Invoked after a successful MakeCredential request.
    virtual void OnPasskeyCreated(
        const sync_pb::WebauthnCredentialSpecifics& passkey) = 0;
  };

  // `delegate`, `model` and `enclave_manager` must be non-null and outlive the
  // transaction. If `uv_method` is `kPIN` `pin` must be non-null.
  // `selected_credential_id` must be non-null iff `request_type` is
  // `kGetAssertion`.
  GPMEnclaveTransaction(
      Delegate* delegate,
      webauthn::PasskeyModel* model,
      device::FidoRequestType request_type,
      std::string rp_id,
      EnclaveUserVerificationMethod uv_method,
      EnclaveManager* enclave_manager,
      std::optional<std::string> pin,
      std::optional<std::vector<uint8_t>> selected_credential_id,
      EnclaveRequestCallback enclave_request_callback);

  ~GPMEnclaveTransaction();

  // Dispatches the request to the enclave authenticator.
  void Start();

 private:
  // Called when the UI has reached a state where it needs to do an enclave
  // operation, and an OAuth token for the enclave has been fetched.
  void MaybeHashPinAndStartEnclaveTransaction(std::optional<std::string> token);

  // Called when the UI has reached a state where it needs to do an enclave
  // operation, an OAuth token for the enclave has been fetched, and any PIN
  // hashing has been completed.
  void StartEnclaveTransaction(std::optional<std::string> token,
                               std::unique_ptr<device::enclave::ClaimedPIN>);

  void HandlePINValidationResult(device::enclave::PINValidationResult result);
  void OnPasskeyCreated(sync_pb::WebauthnCredentialSpecifics passkey);

  raw_ptr<Delegate> delegate_;
  raw_ptr<webauthn::PasskeyModel> passkey_model_;
  device::FidoRequestType request_type_;
  std::string rp_id_;
  EnclaveUserVerificationMethod uv_method_;
  raw_ptr<EnclaveManager> enclave_manager_;
  std::optional<std::string> pin_;
  std::optional<std::vector<uint8_t>> selected_credential_id_;
  EnclaveRequestCallback enclave_request_callback_;

  // The pending request to fetch an OAuth token for the enclave request.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  base::WeakPtrFactory<GPMEnclaveTransaction> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_ENCLAVE_TRANSACTION_H_
