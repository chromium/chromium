// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_enclave_transaction.h"

#include <tuple>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/webauthn/user_actions.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "components/device_event_log/device_event_log.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "device/fido/enclave/types.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"

namespace {

void MaybeRecordUserActionForWinUv(device::FidoRequestType request_type,
                                   EnclaveUserVerificationMethod uv_method) {
#if BUILDFLAG(IS_WIN)
  if (uv_method == EnclaveUserVerificationMethod::kUVKeyWithSystemUI ||
      uv_method == EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI) {
    webauthn::user_actions::RecordGpmWinUvShown(request_type);
  }
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace

GPMEnclaveTransaction::GPMEnclaveTransaction(
    Delegate* delegate,
    webauthn::PasskeyModel* passkey_model,
    device::FidoRequestType request_type,
    std::string rp_id,
    EnclaveUserVerificationMethod uv_method,
    EnclaveManager* enclave_manager,
    std::optional<std::string> pin,
    std::optional<std::vector<uint8_t>> selected_credential_id,
    EnclaveRequestCallback enclave_request_callback)
    : delegate_(delegate),
      passkey_model_(passkey_model),
      request_type_(request_type),
      rp_id_(std::move(rp_id)),
      uv_method_(uv_method),
      enclave_manager_(enclave_manager),
      pin_(std::move(pin)),
      selected_credential_id_(std::move(selected_credential_id)),
      enclave_request_callback_(std::move(enclave_request_callback)) {
  CHECK(delegate_);
  CHECK(passkey_model_);
  CHECK(enclave_manager_);
  CHECK(uv_method != EnclaveUserVerificationMethod::kPIN || pin_.has_value());
  CHECK((request_type_ == device::FidoRequestType::kMakeCredential) ^
        selected_credential_id_.has_value());
  CHECK(enclave_request_callback_);
}

GPMEnclaveTransaction::~GPMEnclaveTransaction() = default;

void GPMEnclaveTransaction::Start() {
  access_token_fetcher_ = enclave_manager_->GetAccessToken(base::BindOnce(
      &GPMEnclaveTransaction::MaybeHashPinAndStartEnclaveTransaction,
      weak_ptr_factory_.GetWeakPtr()));
}

void GPMEnclaveTransaction::MaybeHashPinAndStartEnclaveTransaction(
    std::optional<std::string> token) {
  if (!pin_) {
    StartEnclaveTransaction(std::move(token), nullptr);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(
          [](std::string pin,
             std::unique_ptr<webauthn_pb::EnclaveLocalState_WrappedPIN>
                 wrapped_pin) -> std::unique_ptr<device::enclave::ClaimedPIN> {
            return EnclaveManager::MakeClaimedPINSlowly(std::move(pin),
                                                        std::move(wrapped_pin));
          },
          *pin_, enclave_manager_->GetWrappedPIN()),
      base::BindOnce(&GPMEnclaveTransaction::StartEnclaveTransaction,
                     weak_ptr_factory_.GetWeakPtr(), std::move(token)));
}

void GPMEnclaveTransaction::StartEnclaveTransaction(
    std::optional<std::string> token,
    std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin) {
  // The UI has advanced to the point where it wants to perform an enclave
  // transaction. This code collects the needed values and triggers
  // `enclave_request_callback_` which surfaces in
  // `EnclaveDiscovery::OnUIRequest`.

  if (!token || !enclave_manager_->is_ready()) {
    delegate_->HandleEnclaveTransactionError();
    return;
  }

  auto request = std::make_unique<device::enclave::CredentialRequest>();
  request->access_token = std::move(*token);
  // A request to the enclave can either provide a wrapped secret, which only
  // the enclave can decrypt, or can provide the security domain secret
  // directly. The latter is only possible immediately after registering a
  // device because that's the only time that the actual security domain secret
  // is in memory.
  bool use_unwrapped_secret = false;

  switch (uv_method_) {
    case EnclaveUserVerificationMethod::kNone:
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      break;

    case EnclaveUserVerificationMethod::kImplicit:
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      use_unwrapped_secret = true;
      request->user_verified = true;
      break;

    case EnclaveUserVerificationMethod::kPIN:
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      CHECK(claimed_pin);
      request->claimed_pin = std::move(claimed_pin);
      request->pin_result_callback =
          base::BindOnce(&GPMEnclaveTransaction::HandlePINValidationResult,
                         weak_ptr_factory_.GetWeakPtr());
      request->user_verified = true;
      break;

    case EnclaveUserVerificationMethod::kUVKeyWithChromeUI:
    case EnclaveUserVerificationMethod::kUVKeyWithSystemUI: {
      EnclaveManager::UVKeyOptions uv_options;
      delegate_->BuildUVKeyOptions(uv_options);
      request->signing_callback =
          enclave_manager_->UserVerifyingKeySigningCallback(
              std::move(uv_options));
      request->user_verified = true;
      MaybeRecordUserActionForWinUv(request_type_, uv_method_);
      break;
    }
    case EnclaveUserVerificationMethod::kDeferredUVKeyWithSystemUI:
      // This submits a UV key, but is signed with the HW key. We still count
      // it as being user verified because this will trigger UV creation and
      // the system will verify the user for that operation.
      request->signing_callback =
          enclave_manager_->IdentityKeySigningCallback();
      request->user_verified = true;
      request->uv_key_creation_callback =
          enclave_manager_->UserVerifyingKeyCreationCallback();
      MaybeRecordUserActionForWinUv(request_type_, uv_method_);
      break;
    case EnclaveUserVerificationMethod::kUnsatisfiable:
      NOTREACHED();
  }

  request->unregister_callback =
      base::BindOnce(&EnclaveManager::Unenroll, enclave_manager_->GetWeakPtr(),
                     base::DoNothing());

  switch (request_type_) {
    case device::FidoRequestType::kMakeCredential: {
      if (use_unwrapped_secret) {
        std::tie(request->key_version, request->secret) =
            enclave_manager_->TakeSecret().value();
      } else {
        std::tie(request->key_version, request->wrapped_secret) =
            enclave_manager_->GetCurrentWrappedSecret();
      }
      request->save_passkey_callback =
          base::BindOnce(&GPMEnclaveTransaction::OnPasskeyCreated,
                         weak_ptr_factory_.GetWeakPtr());
      std::vector<std::vector<uint8_t>> existing_credential_ids;
      base::ranges::transform(
          passkey_model_->GetPasskeysForRelyingPartyId(rp_id_),
          std::back_inserter(existing_credential_ids),
          [](const sync_pb::WebauthnCredentialSpecifics& cred) {
            const std::string& cred_id = cred.credential_id();
            return std::vector<uint8_t>(cred_id.begin(), cred_id.end());
          });
      request->existing_cred_ids = std::move(existing_credential_ids);
      break;
    }

    case device::FidoRequestType::kGetAssertion: {
      CHECK(selected_credential_id_);
      std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> selected_credential;
      std::vector<sync_pb::WebauthnCredentialSpecifics> credentials =
          passkey_model_->GetPasskeysForRelyingPartyId(rp_id_);
      for (auto& cred : credentials) {
        if (base::ranges::equal(
                base::as_bytes(base::make_span(cred.credential_id())),
                base::make_span(*selected_credential_id_))) {
          selected_credential =
              std::make_unique<sync_pb::WebauthnCredentialSpecifics>(
                  std::move(cred));
          break;
        }
      }
      CHECK(selected_credential);
      if (base::FeatureList::IsEnabled(device::kWebAuthnUpdateLastUsed)) {
        passkey_model_->UpdatePasskeyTimestamp(
            selected_credential->credential_id(), base::Time::Now());
      }

      if (use_unwrapped_secret) {
        std::tie(std::ignore, request->secret) =
            enclave_manager_->TakeSecret().value();
      } else {
        if (selected_credential->key_version()) {
          std::optional<std::vector<uint8_t>> wrapped_secret =
              enclave_manager_->GetWrappedSecret(
                  selected_credential->key_version());
          if (wrapped_secret) {
            request->wrapped_secret = std::move(*wrapped_secret);
          } else {
            FIDO_LOG(ERROR)
                << "Unexpectedly did not have a wrapped key for epoch "
                << selected_credential->key_version();
          }
        }
        if (!request->wrapped_secret.has_value()) {
          request->wrapped_secret =
              enclave_manager_->GetCurrentWrappedSecret().second;
        }
      }

      request->entity = std::move(selected_credential);
      break;
    }
  }

  CHECK(request->wrapped_secret.has_value() ^ request->secret.has_value());
  enclave_request_callback_.Run(std::move(request));
}

void GPMEnclaveTransaction::HandlePINValidationResult(
    device::enclave::PINValidationResult result) {
  delegate_->HandlePINValidationResult(result);
}

void GPMEnclaveTransaction::OnPasskeyCreated(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  passkey_model_->CreatePasskey(passkey);
  delegate_->OnPasskeyCreated(passkey);
}
