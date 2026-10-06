// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_frontend.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/legacy_standalone_trusted_vault_storage.h"
#include "components/trusted_vault/legacy_standalone_trusted_vault_storage_adapter.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/trusted_vault/trusted_vault_connection_impl.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace trusted_vault {

namespace {

constexpr base::TaskTraits kBackendTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

class IdentityManagerObserver : public signin::IdentityManager::Observer {
 public:
  IdentityManagerObserver(
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      StandaloneTrustedVaultBackend* backend,
      const base::RepeatingClosure& notify_keys_changed_callback,
      signin::IdentityManager* identity_manager);
  IdentityManagerObserver(const IdentityManagerObserver& other) = delete;
  IdentityManagerObserver& operator=(const IdentityManagerObserver& other) =
      delete;
  ~IdentityManagerObserver() override;

  // signin::IdentityManager::Observer implementation.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnAccountsCookieDeletedByUserAction() override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnRefreshTokensLoaded() override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  void UpdatePrimaryAccountIfNeeded();
  void UpdateAccountsInCookieJarInfoIfNeeded(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info);
  StandaloneTrustedVaultBackend::RefreshTokenErrorState
  GetPrimaryAccountRefreshTokenErrorState() const;

  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  const raw_ptr<StandaloneTrustedVaultBackend> backend_;
  const base::RepeatingClosure notify_keys_changed_callback_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  CoreAccountInfo primary_account_;
};

IdentityManagerObserver::IdentityManagerObserver(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    StandaloneTrustedVaultBackend* backend,
    const base::RepeatingClosure& notify_keys_changed_callback,
    signin::IdentityManager* identity_manager)
    : backend_task_runner_(std::move(backend_task_runner)),
      backend_(backend),
      notify_keys_changed_callback_(notify_keys_changed_callback),
      identity_manager_(identity_manager) {
  CHECK(backend_task_runner_);
  CHECK(backend_);
  CHECK(identity_manager_);

  identity_manager_observation_.Observe(identity_manager_);
  UpdatePrimaryAccountIfNeeded();
  if (identity_manager_->AreRefreshTokensLoaded()) {
    OnRefreshTokensLoaded();
  }
}

IdentityManagerObserver::~IdentityManagerObserver() = default;

void IdentityManagerObserver::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  UpdatePrimaryAccountIfNeeded();
}

void IdentityManagerObserver::OnAccountsCookieDeletedByUserAction() {
  // TODO(crbug.com/40156992): remove this handler once tests can mimic
  // OnAccountInCookieUpdated() properly.
  UpdateAccountsInCookieJarInfoIfNeeded(
      signin::AccountsInCookieJarInfo(/*accounts_are_fresh=*/true,
                                      /*accounts=*/{}));
  notify_keys_changed_callback_.Run();
}

void IdentityManagerObserver::OnAccountsInCookieUpdated(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  UpdateAccountsInCookieJarInfoIfNeeded(accounts_in_cookie_jar_info);
  notify_keys_changed_callback_.Run();
}

void IdentityManagerObserver::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  if (primary_account_.IsEmpty() ||
      account_info.account_id != primary_account_.account_id) {
    return;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::SetPrimaryAccount,
                     base::Unretained(backend_), primary_account_,
                     GetPrimaryAccountRefreshTokenErrorState()));
}

void IdentityManagerObserver::OnRefreshTokensLoaded() {
  if (!primary_account_.IsEmpty()) {
    // OnErrorStateOfRefreshTokenUpdatedForAccount() can be called before
    // refresh tokens are marked as loaded, in this case error state can not be
    // identified reliably. To mitigate this, call it again here.
    // It is safe to use the default value for the source of the refresh token
    // operation
    // (`signin_metrics::SourceForRefreshTokenOperation::kUnknown`) as it is not
    // currently used.
    OnErrorStateOfRefreshTokenUpdatedForAccount(
        primary_account_,
        identity_manager_->GetErrorStateOfRefreshTokenForAccount(
            primary_account_.account_id),
        signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  }
  UpdateAccountsInCookieJarInfoIfNeeded(
      identity_manager_->GetAccountsInCookieJar());
}

void IdentityManagerObserver::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  CHECK_EQ(identity_manager, identity_manager_);
  identity_manager_observation_.Reset();
}

void IdentityManagerObserver::UpdatePrimaryAccountIfNeeded() {
  CoreAccountInfo primary_account =
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account == primary_account_) {
    return;
  }
  primary_account_ = primary_account;

  // IdentityManager returns empty CoreAccountInfo if there is no primary
  // account.
  std::optional<CoreAccountInfo> optional_primary_account;
  if (!primary_account.IsEmpty()) {
    optional_primary_account = primary_account;
  }

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::SetPrimaryAccount,
                     base::Unretained(backend_), optional_primary_account,
                     GetPrimaryAccountRefreshTokenErrorState()));
}

void IdentityManagerObserver::UpdateAccountsInCookieJarInfoIfNeeded(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  if (accounts_in_cookie_jar_info.AreAccountsFresh()) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &StandaloneTrustedVaultBackend::UpdateAccountsInCookieJarInfo,
            base::Unretained(backend_), accounts_in_cookie_jar_info));
  }
}

StandaloneTrustedVaultBackend::RefreshTokenErrorState
IdentityManagerObserver::GetPrimaryAccountRefreshTokenErrorState() const {
  if (primary_account_.IsEmpty()) {
    return StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown;
  }

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // Error state of refresh token can't be determined correctly.
    return StandaloneTrustedVaultBackend::RefreshTokenErrorState::kUnknown;
  }

  if (identity_manager_->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_.account_id)) {
    return StandaloneTrustedVaultBackend::RefreshTokenErrorState::
        kPersistentAuthError;
  }
  return StandaloneTrustedVaultBackend::RefreshTokenErrorState::
      kNoPersistentAuthErrors;
}

// Backend delegate that dispatches delegate notifications to custom callbacks,
// used to post notifications from the backend sequence to the UI thread.
class BackendDelegate : public StandaloneTrustedVaultBackend::Delegate {
 public:
  explicit BackendDelegate(base::RepeatingCallback<void(SecurityDomainId)>
                               notify_recoverability_degraded_cb)
      : notify_recoverability_degraded_cb_(
            std::move(notify_recoverability_degraded_cb)) {}

  ~BackendDelegate() override = default;

  // StandaloneTrustedVaultBackend::Delegate implementation.
  void NotifyRecoverabilityDegradedChanged(
      SecurityDomainId security_domain) override {
    notify_recoverability_degraded_cb_.Run(security_domain);
  }

 private:
  const base::RepeatingCallback<void(SecurityDomainId)>
      notify_recoverability_degraded_cb_;
};

}  // namespace

StandaloneTrustedVaultFrontend::StandaloneTrustedVaultFrontend(
#if BUILDFLAG(IS_MAC)
    const std::string& icloud_keychain_access_group_prefix,
#endif
    const base::FilePath& base_dir,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : backend_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBackendTaskTraits)),
      access_token_fetcher_frontend_(identity_manager),
      backend_(nullptr, base::OnTaskRunnerDeleter(backend_task_runner_)) {
  std::unique_ptr<TrustedVaultConnection> connection;
  GURL trusted_vault_service_gurl =
      ExtractTrustedVaultServiceURLFromCommandLine();
  if (trusted_vault_service_gurl.is_valid()) {
    connection = std::make_unique<TrustedVaultConnectionImpl>(
        trusted_vault_service_gurl, url_loader_factory->Clone(),
        std::make_unique<TrustedVaultAccessTokenFetcherImpl>(
            access_token_fetcher_frontend_.GetWeakPtr()));
  }

  backend_.reset(new StandaloneTrustedVaultBackend(
#if BUILDFLAG(IS_MAC)
      icloud_keychain_access_group_prefix,
#endif
      std::make_unique<LegacyStandaloneTrustedVaultStorageAdapter>(
          std::make_unique<LegacyStandaloneTrustedVaultStorage>(
              base_dir, SecurityDomainId::kChromeSync)),
      std::make_unique<BackendDelegate>(base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&StandaloneTrustedVaultFrontend::
                                  NotifyRecoverabilityDegradedChanged,
                              weak_ptr_factory_.GetWeakPtr()))),
      std::move(connection)));

  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     base::Unretained(backend_.get())));

  // Using base::Unretained() is safe here, because |identity_manager_observer_|
  // is owned by |this|.
  identity_manager_observer_ = std::make_unique<IdentityManagerObserver>(
      backend_task_runner_, backend_.get(),
      base::BindRepeating(
          &StandaloneTrustedVaultFrontend::NotifyTrustedVaultKeysChanged,
          base::Unretained(this), std::nullopt),
      identity_manager);
}

StandaloneTrustedVaultFrontend::~StandaloneTrustedVaultFrontend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(clients_.empty());
}

void StandaloneTrustedVaultFrontend::RegisterClient(
    SecurityDomainId security_domain,
    StandaloneTrustedVaultClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(client);
  auto [it, inserted] = clients_.try_emplace(security_domain, client);
  CHECK(inserted);
}

void StandaloneTrustedVaultFrontend::UnregisterClient(
    SecurityDomainId security_domain,
    StandaloneTrustedVaultClient* client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = clients_.find(security_domain);
  CHECK(it != clients_.end());
  CHECK_EQ(it->second, client);
  clients_.erase(it);
}

void StandaloneTrustedVaultFrontend::FetchKeys(
    const CoreAccountInfo& account_info,
    SecurityDomainId security_domain,
    base::OnceCallback<void(const std::vector<std::vector<uint8_t>>&)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  // TODO(crbug.com/40255601): consider notifying observers when FetchKeys()
  // downloads new keys from the server.
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::FetchKeys,
                     base::Unretained(backend_.get()), account_info,
                     security_domain,
                     base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void StandaloneTrustedVaultFrontend::StoreKeys(
    const GaiaId& gaia_id,
    SecurityDomainId security_domain,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&StandaloneTrustedVaultBackend::StoreKeys,
                                base::Unretained(backend_.get()), gaia_id,
                                security_domain, keys, last_key_version));
}

void StandaloneTrustedVaultFrontend::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    SecurityDomainId security_domain,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::MarkLocalKeysAsStale,
                     base::Unretained(backend_.get()), account_info,
                     security_domain),
      std::move(cb));
}

void StandaloneTrustedVaultFrontend::GetIsRecoverabilityDegraded(
    const CoreAccountInfo& account_info,
    SecurityDomainId security_domain,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetIsRecoverabilityDegraded,
          base::Unretained(backend_.get()), account_info, security_domain,
          base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void StandaloneTrustedVaultFrontend::AddTrustedRecoveryMethod(
    const GaiaId& gaia_id,
    SecurityDomainId security_domain,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::AddTrustedRecoveryMethod,
                     base::Unretained(backend_.get()), gaia_id, security_domain,
                     public_key, method_type_hint,
                     base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void StandaloneTrustedVaultFrontend::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ClearLocalDataForAccount,
                     base::Unretained(backend_.get()), account_info));
}

void StandaloneTrustedVaultFrontend::WaitForIdleForTesting(
    base::OnceClosure cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::WaitForIdleForTesting,
                     base::Unretained(backend_.get()),
                     base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void StandaloneTrustedVaultFrontend::
    GetLastAddedRecoveryMethodPublicKeyForTesting(
        base::OnceCallback<void(const std::vector<uint8_t>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::
                         GetLastAddedRecoveryMethodPublicKeyForTesting,
                     base::Unretained(backend_.get())),
      std::move(callback));
}

void StandaloneTrustedVaultFrontend::GetLastKeyVersionForTesting(
    const GaiaId& gaia_id,
    SecurityDomainId security_domain,
    base::OnceCallback<void(int last_key_version)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetLastKeyVersionForTesting,
          base::Unretained(backend_.get()), gaia_id, security_domain),
      std::move(callback));
}

void StandaloneTrustedVaultFrontend::NotifyTrustedVaultKeysChanged(
    std::optional<TrustedVaultUserActionTriggerForUMA> trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [domain, client] : clients_) {
    client->NotifyTrustedVaultKeysChanged(trigger);
  }
}

void StandaloneTrustedVaultFrontend::NotifyRecoverabilityDegradedChanged(
    SecurityDomainId security_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = clients_.find(security_domain);
  if (it != clients_.end()) {
    it->second->NotifyRecoverabilityDegradedChanged();
  }
}

}  // namespace trusted_vault
