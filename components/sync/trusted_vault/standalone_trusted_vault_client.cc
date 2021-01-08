// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/bind_to_task_runner.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"
#include "components/sync/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/sync/trusted_vault/trusted_vault_connection_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {

namespace {

constexpr base::TaskTraits kBackendTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

GURL ExtractTrustedVaultServiceURLFromCommandLine() {
  std::string string_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kTrustedVaultServiceURL);
  if (string_url.empty()) {
    // Command line switch is not specified or is not a valid ASCII string.
    return GURL();
  }
  return GURL(string_url);
}

class PrimaryAccountObserver : public signin::IdentityManager::Observer {
 public:
  PrimaryAccountObserver(
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      scoped_refptr<StandaloneTrustedVaultBackend> backend,
      signin::IdentityManager* identity_manager);
  PrimaryAccountObserver(const PrimaryAccountObserver& other) = delete;
  PrimaryAccountObserver& operator=(const PrimaryAccountObserver& other) =
      delete;
  ~PrimaryAccountObserver() override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  void UpdatePrimaryAccountIfNeeded();

  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  const scoped_refptr<StandaloneTrustedVaultBackend> backend_;
  signin::IdentityManager* const identity_manager_;
  CoreAccountInfo primary_account_;
};

PrimaryAccountObserver::PrimaryAccountObserver(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<StandaloneTrustedVaultBackend> backend,
    signin::IdentityManager* identity_manager)
    : backend_task_runner_(backend_task_runner),
      backend_(backend),
      identity_manager_(identity_manager) {
  DCHECK(backend_task_runner_);
  DCHECK(backend_);
  DCHECK(identity_manager_);

  identity_manager_->AddObserver(this);
  UpdatePrimaryAccountIfNeeded();
}

PrimaryAccountObserver::~PrimaryAccountObserver() {
  identity_manager_->RemoveObserver(this);
}

void PrimaryAccountObserver::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdatePrimaryAccountIfNeeded();
}

void PrimaryAccountObserver::UpdatePrimaryAccountIfNeeded() {
  CoreAccountInfo primary_account = identity_manager_->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);
  if (primary_account == primary_account_) {
    return;
  }
  primary_account_ = primary_account;

  // IdentityManager returns empty CoreAccountInfo if there is no primary
  // account.
  base::Optional<CoreAccountInfo> optional_primary_account;
  if (!primary_account_.IsEmpty()) {
    optional_primary_account = primary_account_;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::SetPrimaryAccount,
                     backend_, optional_primary_account));
}

// Backend delegate that dispatches delegate notifications to custom callbacks,
// used to post notifications from the backend sequence to the UI thread.
class BackendDelegate : public StandaloneTrustedVaultBackend::Delegate {
 public:
  explicit BackendDelegate(
      const base::RepeatingClosure& notify_recoverability_degraded_cb)
      : notify_recoverability_degraded_cb_(notify_recoverability_degraded_cb) {}

  ~BackendDelegate() override = default;

  // StandaloneTrustedVaultBackend::Delegate implementation.
  void NotifyRecoverabilityDegradedChanged() override {
    notify_recoverability_degraded_cb_.Run();
  }

 private:
  const base::RepeatingClosure notify_recoverability_degraded_cb_;
};

}  // namespace

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    const base::FilePath& file_path,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : backend_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBackendTaskTraits)),
      access_token_fetcher_frontend_(identity_manager) {
  if (!base::FeatureList::IsEnabled(
          switches::kSyncSupportTrustedVaultPassphrase)) {
    return;
  }

  std::unique_ptr<TrustedVaultConnection> connection;
  GURL trusted_vault_service_gurl =
      ExtractTrustedVaultServiceURLFromCommandLine();
  if (trusted_vault_service_gurl.is_valid()) {
    connection = std::make_unique<TrustedVaultConnectionImpl>(
        trusted_vault_service_gurl, url_loader_factory->Clone(),
        std::make_unique<TrustedVaultAccessTokenFetcherImpl>(
            access_token_fetcher_frontend_.GetWeakPtr()));
  }

  backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      file_path,
      std::make_unique<
          BackendDelegate>(BindToCurrentSequence(base::BindRepeating(
          &StandaloneTrustedVaultClient::NotifyRecoverabilityDegradedChanged,
          weak_ptr_factory_.GetWeakPtr()))),
      std::move(connection));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     backend_));
  primary_account_observer_ = std::make_unique<PrimaryAccountObserver>(
      backend_task_runner_, backend_, identity_manager);
}

StandaloneTrustedVaultClient::~StandaloneTrustedVaultClient() = default;

void StandaloneTrustedVaultClient::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void StandaloneTrustedVaultClient::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void StandaloneTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::FetchKeys, backend_,
                     account_info, BindToCurrentSequence(std::move(cb))));
}

void StandaloneTrustedVaultClient::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StandaloneTrustedVaultBackend::StoreKeys,
                                backend_, gaia_id, keys, last_key_version));
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void StandaloneTrustedVaultClient::RemoveAllStoredKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::RemoveAllStoredKeys,
                     backend_));
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void StandaloneTrustedVaultClient::MarkKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::MarkKeysAsStale, backend_,
                     account_info),
      std::move(cb));
}

void StandaloneTrustedVaultClient::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetIsRecoverabilityDegraded, backend_,
          account_info, BindToCurrentSequence(std::move(cb))));
}

void StandaloneTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::AddTrustedRecoveryMethod,
                     backend_, gaia_id, public_key,
                     BindToCurrentSequence(std::move(cb))));
}

void StandaloneTrustedVaultClient::WaitForFlushForTesting(
    base::OnceClosure cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(cb));
}

void StandaloneTrustedVaultClient::FetchBackendPrimaryAccountForTesting(
    base::OnceCallback<void(const base::Optional<CoreAccountInfo>&)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting,
          backend_),
      std::move(cb));
}

void StandaloneTrustedVaultClient::SetRecoverabilityDegradedForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::SetRecoverabilityDegradedForTesting,
          backend_));
}

void StandaloneTrustedVaultClient::NotifyRecoverabilityDegradedChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

}  // namespace syncer
