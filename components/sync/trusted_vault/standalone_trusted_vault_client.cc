// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/bind_to_task_runner.h"
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
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;
  void OnUnconsentedPrimaryAccountChanged(
      const CoreAccountInfo& unconsented_primary_account_info) override;

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

void PrimaryAccountObserver::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  UpdatePrimaryAccountIfNeeded();
}

void PrimaryAccountObserver::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  UpdatePrimaryAccountIfNeeded();
}

void PrimaryAccountObserver::OnUnconsentedPrimaryAccountChanged(
    const CoreAccountInfo& unconsented_primary_account_info) {
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

}  // namespace

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    const base::FilePath& file_path,
    signin::IdentityManager* identity_manager)
    : backend_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBackendTaskTraits)),
      access_token_fetcher_frontend_(identity_manager) {
  if (!base::FeatureList::IsEnabled(
          switches::kSyncSupportTrustedVaultPassphrase)) {
    return;
  }
  // TODO(crbug.com/1113598): populate URLLoaderFactory into
  // TrustedVaultConnectionImpl ctor.
  // TODO(crbug.com/1102340): allow setting custom TrustedVaultConnection for
  // testing.
  backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      file_path, std::make_unique<TrustedVaultConnectionImpl>(
                     /*url_loader_factory=*/nullptr,
                     std::make_unique<TrustedVaultAccessTokenFetcherImpl>(
                         access_token_fetcher_frontend_.GetWeakPtr())));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     backend_));
  primary_account_observer_ = std::make_unique<PrimaryAccountObserver>(
      backend_task_runner_, backend_, identity_manager);
}

StandaloneTrustedVaultClient::~StandaloneTrustedVaultClient() = default;

std::unique_ptr<StandaloneTrustedVaultClient::Subscription>
StandaloneTrustedVaultClient::AddKeysChangedObserver(
    const base::RepeatingClosure& cb) {
  return observer_list_.Add(cb);
}

void StandaloneTrustedVaultClient::FetchKeys(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
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
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StandaloneTrustedVaultBackend::StoreKeys,
                                backend_, gaia_id, keys, last_key_version));
  observer_list_.Notify();
}

void StandaloneTrustedVaultClient::RemoveAllStoredKeys() {
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::RemoveAllStoredKeys,
                     backend_));
  observer_list_.Notify();
}

void StandaloneTrustedVaultClient::MarkKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
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
  // TODO(crbug.com/1081649): Implement logic.
  NOTIMPLEMENTED();
  std::move(cb).Run(is_recoverability_degraded_for_testing_);
}

void StandaloneTrustedVaultClient::WaitForFlushForTesting(
    base::OnceClosure cb) const {
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(cb));
}

void StandaloneTrustedVaultClient::FetchBackendPrimaryAccountForTesting(
    base::OnceCallback<void(const base::Optional<CoreAccountInfo>&)> cb) const {
  DCHECK(backend_);
  base::PostTaskAndReplyWithResult(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting,
          backend_),
      std::move(cb));
}

void StandaloneTrustedVaultClient::SetRecoverabilityDegradedForTesting() {
  is_recoverability_degraded_for_testing_ = true;
}

}  // namespace syncer
