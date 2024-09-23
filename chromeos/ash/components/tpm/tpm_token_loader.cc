// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tpm/tpm_token_loader.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/tpm/tpm_token_info_getter.h"
#include "crypto/nss_util.h"

namespace ash {

namespace {

void PostResultToTaskRunner(scoped_refptr<base::SequencedTaskRunner> runner,
                            base::OnceCallback<void(bool)> callback,
                            bool success) {
  runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback), success));
}

}  // namespace

static TPMTokenLoader* g_tpm_token_loader = NULL;

// static
void TPMTokenLoader::Initialize() {
  CHECK(!g_tpm_token_loader);
  g_tpm_token_loader = new TPMTokenLoader(/*initialized_for_test=*/false);
}

// static
void TPMTokenLoader::InitializeForTest() {
  CHECK(!g_tpm_token_loader);
  g_tpm_token_loader = new TPMTokenLoader(/*initialized_for_test=*/true);
}

// static
void TPMTokenLoader::Shutdown() {
  CHECK(g_tpm_token_loader);
  delete g_tpm_token_loader;
  g_tpm_token_loader = NULL;
}

// static
TPMTokenLoader* TPMTokenLoader::Get() {
  CHECK(g_tpm_token_loader)
      << "TPMTokenLoader::Get() called before Initialize()";
  return g_tpm_token_loader;
}

// static
bool TPMTokenLoader::IsInitialized() {
  return g_tpm_token_loader;
}

TPMTokenLoader::TPMTokenLoader(bool initialized_for_test)
    : initialized_for_test_(initialized_for_test),
      tpm_token_state_(TPM_STATE_UNKNOWN),
      tpm_token_info_getter_(TPMTokenInfoGetter::CreateForSystemToken(
          CryptohomePkcs11Client::Get(),
          base::SingleThreadTaskRunner::GetCurrentDefault())),
      tpm_token_slot_id_(-1),
      can_start_before_login_(false) {
  if (!initialized_for_test_ && LoginState::IsInitialized())
    LoginState::Get()->AddObserver(this);

  if (initialized_for_test_) {
    tpm_token_state_ = TPM_TOKEN_INITIALIZED;
    tpm_user_pin_ = "111111";
  }
}

void TPMTokenLoader::SetCryptoTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& crypto_task_runner) {
  crypto_task_runner_ = crypto_task_runner;
  MaybeStartTokenInitialization();
}

void TPMTokenLoader::EnsureStarted() {
  if (can_start_before_login_)
    return;
  can_start_before_login_ = true;
  MaybeStartTokenInitialization();
}

TPMTokenLoader::~TPMTokenLoader() {
  if (!initialized_for_test_ && LoginState::IsInitialized())
    LoginState::Get()->RemoveObserver(this);
}

bool TPMTokenLoader::IsTPMLoadingEnabled() const {
  // TPM loading is enabled on non-ChromeOS environments, e.g. when running
  // tests on Linux.
  // Treat TPM as disabled for guest users since they do not store certs.
  return initialized_for_test_ || enable_tpm_loading_for_testing_ ||
         (base::SysInfo::IsRunningOnChromeOS() &&
          !LoginState::Get()->IsGuestSessionUser());
}

void TPMTokenLoader::MaybeStartTokenInitialization() {
  CHECK(thread_checker_.CalledOnValidThread());

  // This is the entry point to the TPM token initialization process,
  // which we should do at most once.
  if (tpm_token_state_ != TPM_STATE_UNKNOWN || !crypto_task_runner_.get())
    return;

  bool start_initialization =
      (LoginState::IsInitialized() && LoginState::Get()->IsUserLoggedIn()) ||
      can_start_before_login_;

  VLOG(1) << "StartTokenInitialization: " << start_initialization;
  if (!start_initialization)
    return;

  if (!IsTPMLoadingEnabled())
    tpm_token_state_ = TPM_DISABLED;

  ContinueTokenInitialization();

  DCHECK_NE(tpm_token_state_, TPM_STATE_UNKNOWN);
}

void TPMTokenLoader::ContinueTokenInitialization() {
  CHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "ContinueTokenInitialization: " << tpm_token_state_;

  switch (tpm_token_state_) {
    case TPM_STATE_UNKNOWN: {
      tpm_token_state_ = TPM_INITIALIZATION_STARTED;
      tpm_token_info_getter_->Start(base::BindOnce(
          &TPMTokenLoader::OnGotTpmTokenInfo, weak_factory_.GetWeakPtr()));
      return;
    }
    case TPM_INITIALIZATION_STARTED: {
      NOTREACHED_IN_MIGRATION();
      return;
    }
    case TPM_TOKEN_INFO_RECEIVED: {
      crypto_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              &crypto::InitializeTPMTokenAndSystemSlot, tpm_token_slot_id_,
              base::BindOnce(
                  &PostResultToTaskRunner,
                  base::SingleThreadTaskRunner::GetCurrentDefault(),
                  base::BindOnce(&TPMTokenLoader::OnTPMTokenInitialized,
                                 weak_factory_.GetWeakPtr()))));
      return;
    }
    case TPM_TOKEN_INITIALIZED:
    case TPM_DISABLED: {
      NotifyTPMTokenReady();
      return;
    }
  }
}

void TPMTokenLoader::OnGotTpmTokenInfo(
    std::optional<user_data_auth::TpmTokenInfo> token_info) {
  if (!token_info.has_value()) {
    tpm_token_state_ = TPM_DISABLED;
    ContinueTokenInitialization();
    return;
  }

  tpm_token_slot_id_ = token_info->slot();
  tpm_user_pin_ = token_info->user_pin();
  tpm_token_state_ = TPM_TOKEN_INFO_RECEIVED;

  ContinueTokenInitialization();
}

void TPMTokenLoader::OnTPMTokenInitialized(bool success) {
  VLOG(1) << "OnTPMTokenInitialized: " << success;

  tpm_token_state_ = success ? TPM_TOKEN_INITIALIZED : TPM_DISABLED;
  ContinueTokenInitialization();
}

void TPMTokenLoader::NotifyTPMTokenReady() {
  crypto_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&crypto::FinishInitializingTPMTokenAndSystemSlot));
}

void TPMTokenLoader::LoggedInStateChanged() {
  VLOG(1) << "LoggedInStateChanged";
  MaybeStartTokenInitialization();
}

}  // namespace ash
