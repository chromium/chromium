// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_runner.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/tpm/buildflags.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

namespace ash {

namespace {

const int64_t kInitialRequestDelayMs = 100;
const int64_t kMaxRequestDelayMs = 300000;  // 5 minutes

#if BUILDFLAG(NSS_SLOTS_SOFTWARE_FALLBACK)
constexpr bool kIsSystemSlotSoftwareFallbackAllowed = true;
#else
constexpr bool kIsSystemSlotSoftwareFallbackAllowed = false;
#endif

// Calculates the delay before running next attempt to initiatialize the TPM
// token, if |last_delay| was the last or initial delay.
base::TimeDelta GetNextRequestDelayMs(base::TimeDelta last_delay) {
  // This implements an exponential backoff, as we don't know in which order of
  // magnitude the TPM token changes it's state.
  base::TimeDelta next_delay = last_delay * 2;

  // Cap the delay to prevent an overflow. This threshold is arbitrarily chosen.
  const base::TimeDelta max_delay = base::Milliseconds(kMaxRequestDelayMs);
  if (next_delay > max_delay)
    next_delay = max_delay;
  return next_delay;
}

}  // namespace

// static
std::unique_ptr<TPMTokenInfoGetter> TPMTokenInfoGetter::CreateForUserToken(
    const AccountId& account_id,
    CryptohomePkcs11Client* userdataauth_client,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner) {
  CHECK(account_id.is_valid());
  return base::WrapUnique(new TPMTokenInfoGetter(
      TYPE_USER, account_id, userdataauth_client, delayed_task_runner));
}

// static
std::unique_ptr<TPMTokenInfoGetter> TPMTokenInfoGetter::CreateForSystemToken(
    CryptohomePkcs11Client* userdataauth_client,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner) {
  return base::WrapUnique(new TPMTokenInfoGetter(
      TYPE_SYSTEM, EmptyAccountId(), userdataauth_client, delayed_task_runner));
}

TPMTokenInfoGetter::~TPMTokenInfoGetter() = default;

void TPMTokenInfoGetter::Start(TpmTokenInfoCallback callback) {
  CHECK(state_ == STATE_INITIAL);
  CHECK(!callback.is_null());

  callback_ = std::move(callback);

  state_ = STATE_STARTED;
  Continue();
}

TPMTokenInfoGetter::TPMTokenInfoGetter(
    TPMTokenInfoGetter::Type type,
    const AccountId& account_id,
    CryptohomePkcs11Client* cryptohome_pkcs11_client,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner)
    : delayed_task_runner_(delayed_task_runner),
      type_(type),
      state_(TPMTokenInfoGetter::STATE_INITIAL),
      account_id_(account_id),
      use_nss_slots_software_fallback_(kIsSystemSlotSoftwareFallbackAllowed),
      tpm_request_delay_(base::Milliseconds(kInitialRequestDelayMs)),
      cryptohome_pkcs11_client_(cryptohome_pkcs11_client) {}

void TPMTokenInfoGetter::Continue() {
  user_data_auth::Pkcs11GetTpmTokenInfoRequest request;
  switch (state_) {
    case STATE_INITIAL:
      NOTREACHED_IN_MIGRATION();
      break;
    case STATE_STARTED:
      chromeos::TpmManagerClient::Get()->GetTpmNonsensitiveStatus(
          ::tpm_manager::GetTpmNonsensitiveStatusRequest(),
          base::BindOnce(&TPMTokenInfoGetter::OnGetTpmStatus,
                         weak_factory_.GetWeakPtr()));
      break;
    case STATE_TPM_ENABLED:
    case STATE_NSS_SLOTS_SOFTWARE_FALLBACK:
      // For system token, we don't need to supply the username, and with an
      // empty username, cryptohomed will return the system token information.
      if (type_ == TYPE_USER) {
        request.set_username(
            cryptohome::CreateAccountIdentifierFromAccountId(account_id_)
                .account_id());
      }
      cryptohome_pkcs11_client_->Pkcs11GetTpmTokenInfo(
          request, base::BindOnce(&TPMTokenInfoGetter::OnPkcs11GetTpmTokenInfo,
                                  weak_factory_.GetWeakPtr()));
      break;
    case STATE_DONE:
      NOTREACHED_IN_MIGRATION();
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

  // In case the use_nss_slots_software_fallback_ is true and the TPM is not
  // owned, we continue the token info retrieval for the nss slots in order to
  // fall back to a software-backed initialization.
  if (use_nss_slots_software_fallback_ && !reply.is_owned()) {
    state_ = STATE_NSS_SLOTS_SOFTWARE_FALLBACK;
    Continue();
    return;
  }

  if (!reply.is_enabled()) {
    state_ = STATE_DONE;
    std::move(callback_).Run(std::nullopt);
    return;
  }

  state_ = STATE_TPM_ENABLED;
  Continue();
}

void TPMTokenInfoGetter::OnPkcs11GetTpmTokenInfo(
    std::optional<user_data_auth::Pkcs11GetTpmTokenInfoReply> token_info) {
  if (!token_info.has_value() || !token_info->has_token_info() ||
      token_info->token_info().slot() == -1) {
    RetryLater();
    return;
  }

  state_ = STATE_DONE;
  std::move(callback_).Run(token_info->token_info());
}

}  // namespace ash
