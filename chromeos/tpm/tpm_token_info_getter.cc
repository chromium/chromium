// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/tpm/tpm_token_info_getter.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

namespace {

const int64_t kInitialRequestDelayMs = 100;
const int64_t kMaxRequestDelayMs = 300000;  // 5 minutes

// Calculates the delay before running next attempt to initiatialize the TPM
// token, if |last_delay| was the last or initial delay.
base::TimeDelta GetNextRequestDelayMs(base::TimeDelta last_delay) {
  // This implements an exponential backoff, as we don't know in which order of
  // magnitude the TPM token changes it's state.
  base::TimeDelta next_delay = last_delay * 2;

  // Cap the delay to prevent an overflow. This threshold is arbitrarily chosen.
  const base::TimeDelta max_delay =
      base::TimeDelta::FromMilliseconds(kMaxRequestDelayMs);
  if (next_delay > max_delay)
    next_delay = max_delay;
  return next_delay;
}

}  // namespace

namespace chromeos {

// static
std::unique_ptr<TPMTokenInfoGetter> TPMTokenInfoGetter::CreateForUserToken(
    const AccountId& account_id,
    CryptohomeClient* cryptohome_client,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner) {
  CHECK(account_id.is_valid());
  return std::unique_ptr<TPMTokenInfoGetter>(new TPMTokenInfoGetter(
      TYPE_USER, account_id, cryptohome_client, delayed_task_runner));
}

// static
std::unique_ptr<TPMTokenInfoGetter> TPMTokenInfoGetter::CreateForSystemToken(
    CryptohomeClient* cryptohome_client,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner) {
  return std::unique_ptr<TPMTokenInfoGetter>(new TPMTokenInfoGetter(
      TYPE_SYSTEM, EmptyAccountId(), cryptohome_client, delayed_task_runner));
}

TPMTokenInfoGetter::~TPMTokenInfoGetter() = default;

void TPMTokenInfoGetter::Start(TpmTokenInfoCallback callback) {
  CHECK(state_ == STATE_INITIAL);
  CHECK(!callback.is_null());

  callback_ = std::move(callback);

  state_ = STATE_STARTED;
  Continue();
}

void TPMTokenInfoGetter::SetSystemSlotSoftwareFallback(
    bool use_system_slot_software_fallback) {
  use_system_slot_software_fallback_ = use_system_slot_software_fallback;
}

TPMTokenInfoGetter::TPMTokenInfoGetter(
    TPMTokenInfoGetter::Type type,
    const AccountId& account_id,
    CryptohomeClient* cryptohome_client,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner)
    : delayed_task_runner_(delayed_task_runner),
      type_(type),
      state_(TPMTokenInfoGetter::STATE_INITIAL),
      account_id_(account_id),
      tpm_request_delay_(
          base::TimeDelta::FromMilliseconds(kInitialRequestDelayMs)),
      cryptohome_client_(cryptohome_client) {}

void TPMTokenInfoGetter::Continue() {
  switch (state_) {
    case STATE_INITIAL:
      NOTREACHED();
      break;
    case STATE_STARTED:
      TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
          ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
          base::BindOnce(&TPMTokenInfoGetter::OnGetTpmStatus,
                         weak_factory_.GetWeakPtr()));
      break;
    case STATE_TPM_ENABLED:
      if (type_ == TYPE_SYSTEM) {
        cryptohome_client_->Pkcs11GetTpmTokenInfo(
            base::BindOnce(&TPMTokenInfoGetter::OnPkcs11GetTpmTokenInfo,
                           weak_factory_.GetWeakPtr()));
      } else {  // if (type_ == TYPE_USER)
        cryptohome_client_->Pkcs11GetTpmTokenInfoForUser(
            cryptohome::CreateAccountIdentifierFromAccountId(account_id_),
            base::BindOnce(&TPMTokenInfoGetter::OnPkcs11GetTpmTokenInfo,
                           weak_factory_.GetWeakPtr()));
      }
      break;
    case STATE_SYSTEM_SLOT_SOFTWARE_FALLBACK:
      if (type_ == TYPE_SYSTEM) {
        cryptohome_client_->Pkcs11GetTpmTokenInfo(
            base::BindOnce(&TPMTokenInfoGetter::OnPkcs11GetTpmTokenInfo,
                           weak_factory_.GetWeakPtr()));
      } else {  // if (type_ == TYPE_USER)
        NOTREACHED();
      }
      break;
    case STATE_DONE:
      NOTREACHED();
  }
}

void TPMTokenInfoGetter::RetryLater() {
  delayed_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TPMTokenInfoGetter::Continue, weak_factory_.GetWeakPtr()),
      tpm_request_delay_);
  tpm_request_delay_ = GetNextRequestDelayMs(tpm_request_delay_);
}

void TPMTokenInfoGetter::OnGetTpmStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply) {
  if (reply.status() != ::tpm_manager::STATUS_SUCCESS) {
    LOG(WARNING) << "Failed to get tpm status; status: " << reply.status();
    RetryLater();
    return;
  }

  if (!reply.is_enabled()) {
    // In case the TPM is disabled and use_system_slot_software_fallback_ is
    // true, we continue the token info retrieval for the system slot in order
    // to fall back to a software-backed initialization.
    if (use_system_slot_software_fallback_) {
      state_ = STATE_SYSTEM_SLOT_SOFTWARE_FALLBACK;
      Continue();
      return;
    }

    state_ = STATE_DONE;
    std::move(callback_).Run(base::nullopt);
    return;
  }

  state_ = STATE_TPM_ENABLED;
  Continue();
}

void TPMTokenInfoGetter::OnPkcs11GetTpmTokenInfo(
    base::Optional<CryptohomeClient::TpmTokenInfo> token_info) {
  if (!token_info.has_value() || token_info->slot == -1) {
    RetryLater();
    return;
  }

  state_ = STATE_DONE;
  std::move(callback_).Run(std::move(token_info));
}

}  // namespace chromeos
