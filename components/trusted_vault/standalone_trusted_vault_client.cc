// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/recovery_key_store_connection_impl.h"
#include "components/trusted_vault/recovery_key_store_controller.h"
#include "components/trusted_vault/standalone_trusted_vault_backend.h"
#include "components/trusted_vault/trusted_vault_access_token_fetcher_impl.h"
#include "components/trusted_vault/trusted_vault_connection_impl.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace trusted_vault {

namespace {

constexpr base::TaskTraits kBackendTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

void ReplyToIsDeviceRegisteredForTesting(  // IN-TEST
    base::OnceCallback<void(bool)> is_device_registered_callback,
    const trusted_vault_pb::LocalDeviceRegistrationInfo&
        device_registration_info) {
  std::move(is_device_registered_callback)
      .Run(device_registration_info.device_registered());
}

class IdentityManagerObserver : public signin::IdentityManager::Observer {
 public:
  IdentityManagerObserver(
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      scoped_refptr<StandaloneTrustedVaultBackend> backend,
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

 private:
  void UpdatePrimaryAccountIfNeeded();
  void UpdateAccountsInCookieJarInfoIfNeeded(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info);
  StandaloneTrustedVaultBackend::RefreshTokenErrorState
  GetPrimaryAccountRefreshTokenErrorState() const;

  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  const scoped_refptr<StandaloneTrustedVaultBackend> backend_;
  const base::RepeatingClosure notify_keys_changed_callback_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  CoreAccountInfo primary_account_;
};

IdentityManagerObserver::IdentityManagerObserver(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<StandaloneTrustedVaultBackend> backend,
    const base::RepeatingClosure& notify_keys_changed_callback,
    signin::IdentityManager* identity_manager)
    : backend_task_runner_(backend_task_runner),
      backend_(backend),
      notify_keys_changed_callback_(notify_keys_changed_callback),
      identity_manager_(identity_manager) {
  DCHECK(backend_task_runner_);
  DCHECK(backend_);
  DCHECK(identity_manager_);

  identity_manager_->AddObserver(this);
  UpdatePrimaryAccountIfNeeded();
  if (identity_manager_->AreRefreshTokensLoaded()) {
    OnRefreshTokensLoaded();
  }
}

IdentityManagerObserver::~IdentityManagerObserver() {
  identity_manager_->RemoveObserver(this);
}

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
                     backend_, primary_account_,
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
                     backend_, optional_primary_account,
                     GetPrimaryAccountRefreshTokenErrorState()));
}

void IdentityManagerObserver::UpdateAccountsInCookieJarInfoIfNeeded(
    const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info) {
  if (accounts_in_cookie_jar_info.AreAccountsFresh()) {
    backend_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &StandaloneTrustedVaultBackend::UpdateAccountsInCookieJarInfo,
            backend_, accounts_in_cookie_jar_info));
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
  explicit BackendDelegate(
      const base::RepeatingClosure& notify_recoverability_degraded_cb,
      const base::RepeatingClosure& notify_state_changed_cb)
      : notify_recoverability_degraded_cb_(notify_recoverability_degraded_cb),
        notify_state_changed_cb_(notify_state_changed_cb) {}

  ~BackendDelegate() override = default;

  // StandaloneTrustedVaultBackend::Delegate implementation.
  void NotifyRecoverabilityDegradedChanged() override {
    notify_recoverability_degraded_cb_.Run();
  }

  void NotifyStateChanged() override { notify_state_changed_cb_.Run(); }

 private:
  const base::RepeatingClosure notify_recoverability_degraded_cb_;
  const base::RepeatingClosure notify_state_changed_cb_;
};

constexpr base::FilePath::CharType kChromeSyncTrustedVaultFilename[] =
    FILE_PATH_LITERAL("trusted_vault.pb");
constexpr base::FilePath::CharType kPasskeysTrustedVaultFilename[] =
    FILE_PATH_LITERAL("passkeys_trusted_vault.pb");

base::FilePath GetBackendFilePath(const base::FilePath& base_dir,
                                  SecurityDomainId security_domain) {
  switch (security_domain) {
    case SecurityDomainId::kChromeSync:
      return base_dir.Append(kChromeSyncTrustedVaultFilename);
    case SecurityDomainId::kPasskeys:
      return base_dir.Append(kPasskeysTrustedVaultFilename);
  }
  NOTREACHED();
}

}  // namespace

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    SecurityDomainId security_domain,
    const base::FilePath& base_dir,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<RecoveryKeyStoreController::RecoveryKeyProvider>
        recovery_key_provider)
    : backend_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBackendTaskTraits)),
      access_token_fetcher_frontend_(identity_manager) {
  std::unique_ptr<TrustedVaultConnection> connection;
  GURL trusted_vault_service_gurl =
      ExtractTrustedVaultServiceURLFromCommandLine();
  if (trusted_vault_service_gurl.is_valid()) {
    connection = std::make_unique<TrustedVaultConnectionImpl>(
        security_domain, trusted_vault_service_gurl,
        url_loader_factory->Clone(),
        std::make_unique<TrustedVaultAccessTokenFetcherImpl>(
            access_token_fetcher_frontend_.GetWeakPtr()));
  }

  std::unique_ptr<RecoveryKeyStoreConnection> recovery_key_store_connection;
  if (recovery_key_provider) {
    recovery_key_store_connection =
        std::make_unique<RecoveryKeyStoreConnectionImpl>(
            url_loader_factory->Clone(),
            std::make_unique<TrustedVaultAccessTokenFetcherImpl>(
                access_token_fetcher_frontend_.GetWeakPtr()));
  }

  backend_ = base::MakeRefCounted<StandaloneTrustedVaultBackend>(
      security_domain, GetBackendFilePath(base_dir, security_domain),
      std::make_unique<BackendDelegate>(
          base::BindPostTaskToCurrentDefault(
              base::BindRepeating(&StandaloneTrustedVaultClient::
                                      NotifyRecoverabilityDegradedChanged,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTaskToCurrentDefault(base::BindRepeating(
              &StandaloneTrustedVaultClient::NotifyBackendStateChanged,
              weak_ptr_factory_.GetWeakPtr()))),
      std::move(connection), std::move(recovery_key_provider),
      std::move(recovery_key_store_connection));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ReadDataFromDisk,
                     backend_));
  // Using base::Unretained() is safe here, because |identity_manager_observer_|
  // owned by |this|.
  identity_manager_observer_ = std::make_unique<IdentityManagerObserver>(
      backend_task_runner_, backend_,
      base::BindRepeating(
          &StandaloneTrustedVaultClient::NotifyTrustedVaultKeysChanged,
          base::Unretained(this)),
      identity_manager);
}

StandaloneTrustedVaultClient::StandaloneTrustedVaultClient(
    SecurityDomainId security_domain,
    const base::FilePath& base_dir,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : StandaloneTrustedVaultClient(security_domain,
                                   base_dir,
                                   identity_manager,
                                   url_loader_factory,
                                   /*recovery_key_provider=*/nullptr) {}

StandaloneTrustedVaultClient::~StandaloneTrustedVaultClient() {
  // |backend_| needs to be destroyed inside backend sequence, not the current
  // one. Destroy |identity_manager_observer_| that owns pointer to |backend_|
  // as well and release |backend_| in |backend_task_runner_|.
  identity_manager_observer_.reset();
  backend_task_runner_->ReleaseSoon(FROM_HERE, std::move(backend_));
}

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
                     account_info,
                     base::BindPostTaskToCurrentDefault(std::move(cb))));
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
  NotifyTrustedVaultKeysChanged();
}

void StandaloneTrustedVaultClient::MarkLocalKeysAsStale(
    const CoreAccountInfo& account_info,
    base::OnceCallback<void(bool)> cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::MarkLocalKeysAsStale,
                     backend_, account_info),
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
          account_info, base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void StandaloneTrustedVaultClient::AddTrustedRecoveryMethod(
    const std::string& gaia_id,
    const std::vector<uint8_t>& public_key,
    int method_type_hint,
    base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::AddTrustedRecoveryMethod,
                     backend_, gaia_id, public_key, method_type_hint,
                     base::BindPostTaskToCurrentDefault(std::move(cb))));
}

void StandaloneTrustedVaultClient::ClearLocalDataForAccount(
    const CoreAccountInfo& account_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::ClearLocalDataForAccount,
                     backend_, account_info));
}

void StandaloneTrustedVaultClient::WaitForFlushForTesting(
    base::OnceClosure cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         std::move(cb));
}

void StandaloneTrustedVaultClient::FetchBackendPrimaryAccountForTesting(
    base::OnceCallback<void(const std::optional<CoreAccountInfo>&)> cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetPrimaryAccountForTesting,
          backend_),
      std::move(cb));
}

void StandaloneTrustedVaultClient::FetchIsDeviceRegisteredForTesting(
    const std::string& gaia_id,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetDeviceRegistrationInfoForTesting,
          backend_, gaia_id),
      base::BindOnce(&ReplyToIsDeviceRegisteredForTesting,
                     std::move(callback)));
}

void StandaloneTrustedVaultClient::AddDebugObserverForTesting(
    DebugObserver* debug_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  debug_observer_list_.AddObserver(debug_observer);
}

void StandaloneTrustedVaultClient::RemoveDebugObserverForTesting(
    DebugObserver* debug_observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  debug_observer_list_.RemoveObserver(debug_observer);
}

void StandaloneTrustedVaultClient::
    GetLastAddedRecoveryMethodPublicKeyForTesting(
        base::OnceCallback<void(const std::vector<uint8_t>&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StandaloneTrustedVaultBackend::
                         GetLastAddedRecoveryMethodPublicKeyForTesting,
                     backend_),
      std::move(callback));
}

void StandaloneTrustedVaultClient::GetLastKeyVersionForTesting(
    const std::string& gaia_id,
    base::OnceCallback<void(int last_key_version)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &StandaloneTrustedVaultBackend::GetLastKeyVersionForTesting, backend_,
          gaia_id),
      std::move(callback));
}

void StandaloneTrustedVaultClient::NotifyTrustedVaultKeysChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultKeysChanged();
  }
}

void StandaloneTrustedVaultClient::NotifyRecoverabilityDegradedChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Observer& observer : observer_list_) {
    observer.OnTrustedVaultRecoverabilityChanged();
  }
}

void StandaloneTrustedVaultClient::NotifyBackendStateChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (DebugObserver& debug_observer : debug_observer_list_) {
    debug_observer.OnBackendStateChanged();
  }
}

}  // namespace trusted_vault
