// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/chromeos/account_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/sha1.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/chromeos/tokens.pb.h"
#include "components/account_manager_core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace account_manager {

namespace {

constexpr base::FilePath::CharType kTokensFileName[] =
    FILE_PATH_LITERAL("AccountManagerTokens.bin");
constexpr int kTokensFileMaxSizeInBytes = 100000;  // ~100 KB.

constexpr char kNumAccountsMetricName[] = "AccountManager.NumAccounts";
constexpr int kMaxNumAccountsMetric = 10;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Enums labels are at |AccountManagerTokenLoadStatus|.
enum class TokenLoadStatus {
  kSuccess = 0,
  kFileReadError = 1,
  kFileParseError = 2,
  kAccountCorruptionDetected = 3,
  kMaxValue = kAccountCorruptionDetected,
};

void RecordNumAccountsMetric(const int num_accounts) {
  base::UmaHistogramExactLinear(kNumAccountsMetricName, num_accounts,
                                kMaxNumAccountsMetric + 1);
}

void RecordTokenLoadStatus(const TokenLoadStatus& token_load_status) {
  base::UmaHistogramEnumeration("AccountManager.TokenLoadStatus",
                                token_load_status);
}

void RecordInitializationTime(
    const base::TimeTicks& initialization_start_time) {
  base::UmaHistogramMicrosecondsTimes(
      "AccountManager.InitializationTime",
      base::TimeTicks::Now() - initialization_start_time);
}

// Returns `nullopt` if `account_type` is `ACCOUNT_TYPE_UNSPECIFIED`.
std::optional<::account_manager::AccountType> FromProtoAccountType(
    const internal::AccountType& account_type) {
  switch (account_type) {
    case internal::AccountType::ACCOUNT_TYPE_UNSPECIFIED:
      return std::nullopt;
    case internal::AccountType::ACCOUNT_TYPE_GAIA:
      static_assert(
          static_cast<int>(internal::AccountType::ACCOUNT_TYPE_GAIA) ==
              static_cast<int>(::account_manager::AccountType::kGaia),
          "Underlying enum values must match");
      return ::account_manager::AccountType::kGaia;
    case internal::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY:
      static_assert(static_cast<int>(
                        internal::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY) ==
                        static_cast<int>(
                            ::account_manager::AccountType::kActiveDirectory),
                    "Underlying enum values must match");
      return ::account_manager::AccountType::kActiveDirectory;
  }
}

internal::AccountType ToProtoAccountType(
    const ::account_manager::AccountType& account_type) {
  switch (account_type) {
    case ::account_manager::AccountType::kGaia:
      return internal::AccountType::ACCOUNT_TYPE_GAIA;
    case ::account_manager::AccountType::kActiveDirectory:
      return internal::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY;
  }
}

// Returns a Base16 encoded SHA1 digest of `data`.
std::string Sha1Digest(const std::string& data) {
  return base::HexEncode(base::SHA1Hash(base::as_byte_span(data)));
}

}  // namespace

// static
const char AccountManager::kActiveDirectoryDummyToken[] = "dummy_ad_token";

// static
const char* const AccountManager::kInvalidToken =
    GaiaConstants::kInvalidRefreshToken;

class AccountManager::GaiaTokenRevocationRequest : public GaiaAuthConsumer {
 public:
  GaiaTokenRevocationRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AccountManager::DelayNetworkCallRunner delay_network_call_runner,
      const std::string& refresh_token,
      base::WeakPtr<AccountManager> account_manager)
      : account_manager_(account_manager), refresh_token_(refresh_token) {
    DCHECK(!refresh_token_.empty());
    gaia_auth_fetcher_ = std::make_unique<GaiaAuthFetcher>(
        this, gaia::GaiaSource::kChromeOS, url_loader_factory);
    base::OnceClosure start_revoke_token = base::BindOnce(
        &GaiaTokenRevocationRequest::Start, weak_factory_.GetWeakPtr());
    delay_network_call_runner.Run(std::move(start_revoke_token));
  }
  GaiaTokenRevocationRequest(const GaiaTokenRevocationRequest&) = delete;
  GaiaTokenRevocationRequest& operator=(const GaiaTokenRevocationRequest&) =
      delete;
  ~GaiaTokenRevocationRequest() override = default;

  // GaiaAuthConsumer overrides.
  void OnOAuth2RevokeTokenCompleted(TokenRevocationStatus status) override {
    VLOG(1) << "GaiaTokenRevocationRequest::OnOAuth2RevokeTokenCompleted";
    // We cannot call |AccountManager::DeletePendingTokenRevocationRequest|
    // directly because it will immediately start deleting |this|, before the
    // method has had a chance to return.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&AccountManager::DeletePendingTokenRevocationRequest,
                       account_manager_, this));
  }

 private:
  // Starts the actual work of sending a network request to revoke a token.
  void Start() { gaia_auth_fetcher_->StartRevokeOAuth2Token(refresh_token_); }

  // A weak pointer to |AccountManager|. The only purpose is to signal
  // the completion of work through
  // |AccountManager::DeletePendingTokenRevocationRequest|.
  base::WeakPtr<AccountManager> account_manager_;

  // Does the actual work of revoking a token.
  std::unique_ptr<GaiaAuthFetcher> gaia_auth_fetcher_;

  // Refresh token to be revoked from GAIA.
  std::string refresh_token_;

  base::WeakPtrFactory<GaiaTokenRevocationRequest> weak_factory_{this};
};

class AccountManager::AccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  // Creates an instance of `AccessTokenFetcher`.
  // `account_key` must be a Gaia account.
  // `account_manager` is a non-owning pointer which must outlive `this`
  // instance.
  // `consumer` is a non-owning pointer.
  AccessTokenFetcher(const ::account_manager::AccountKey& account_key,
                     AccountManager* account_manager,
                     OAuth2AccessTokenConsumer* consumer)
      : OAuth2AccessTokenFetcher(consumer),
        account_key_(account_key),
        account_manager_(account_manager),
        consumer_(consumer) {
    DCHECK(account_manager_);
    DCHECK(consumer_);
  }
  AccessTokenFetcher(const AccessTokenFetcher&) = delete;
  AccessTokenFetcher& operator=(const AccessTokenFetcher&) = delete;
  ~AccessTokenFetcher() override = default;

  // Returns a closure which marks `this` instance as ready for use.
  base::OnceClosure UnblockTokenRequest() {
    return base::BindOnce(&AccessTokenFetcher::UnblockTokenRequestInternal,
                          weak_factory_.GetWeakPtr());
  }

  // OAuth2AccessTokenFetcher override:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override {
    DCHECK(!is_request_pending_);
    client_id_ = client_id;
    client_secret_ = client_secret;
    scopes_ = scopes;
    if (!are_token_requests_allowed_) {
      is_request_pending_ = true;
      // This request will be started when the closure returned by
      // `UnblockTokenRequest` is executed.
      return;
    }
    StartInternal();
  }

  // OAuth2AccessTokenFetcher override:
  void CancelRequest() override { access_token_fetcher_.reset(); }

 private:
  void UnblockTokenRequestInternal() {
    are_token_requests_allowed_ = true;
    if (is_request_pending_) {
      StartInternal();
    }
  }

  void StartInternal() {
    DCHECK(are_token_requests_allowed_);
    is_request_pending_ = false;

    if (account_key_.account_type() != ::account_manager::AccountType::kGaia) {
      FireOnGetTokenFailure(GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
      return;
    }

    std::optional<std::string> maybe_token =
        account_manager_->GetRefreshToken(account_key_);
    if (!maybe_token.has_value()) {
      FireOnGetTokenFailure(GoogleServiceAuthError(
          GoogleServiceAuthError::State::USER_NOT_SIGNED_UP));
      return;
    }

    DCHECK(!access_token_fetcher_);
    access_token_fetcher_ = GaiaAccessTokenFetcher::
        CreateExchangeRefreshTokenForAccessTokenInstance(
            consumer_, account_manager_->GetUrlLoaderFactory(),
            maybe_token.value());
    access_token_fetcher_->Start(client_id_, client_secret_, scopes_);
  }

  const ::account_manager::AccountKey account_key_;
  const raw_ptr<AccountManager, DanglingUntriaged> account_manager_;
  const raw_ptr<OAuth2AccessTokenConsumer> consumer_;

  bool are_token_requests_allowed_ = false;
  bool is_request_pending_ = false;
  std::string client_id_;
  std::string client_secret_;
  std::vector<std::string> scopes_;
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;

  base::WeakPtrFactory<AccessTokenFetcher> weak_factory_{this};
};

AccountManager::Observer::Observer() = default;

AccountManager::Observer::~Observer() = default;

AccountManager::AccountManager() = default;

// static
void AccountManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed,
      /*default_value=*/true);
}

void AccountManager::SetPrefService(PrefService* pref_service) {
  DCHECK(pref_service);
  pref_service_ = pref_service;
}

void AccountManager::InitializeInEphemeralMode(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  InitializeInEphemeralMode(url_loader_factory,
                            /* initialization_callback= */
                            base::DoNothing());
}

void AccountManager::InitializeInEphemeralMode(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::OnceClosure initialization_callback) {
  Initialize(/* home_dir= */ base::FilePath(), url_loader_factory,
             /* delay_network_call_runner= */
             base::BindRepeating(
                 [](base::OnceClosure closure) { std::move(closure).Run(); }),
             /* task_runner= */ nullptr, std::move(initialization_callback));
}

void AccountManager::Initialize(
    const base::FilePath& home_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DelayNetworkCallRunner delay_network_call_runner) {
  Initialize(home_dir, url_loader_factory, delay_network_call_runner,
             base::DoNothing());
}

void AccountManager::Initialize(
    const base::FilePath& home_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DelayNetworkCallRunner delay_network_call_runner,
    base::OnceClosure initialization_callback) {
  Initialize(
      home_dir, url_loader_factory, std::move(delay_network_call_runner),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()}),
      std::move(initialization_callback));
}

void AccountManager::Initialize(
    const base::FilePath& home_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DelayNetworkCallRunner delay_network_call_runner,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceClosure initialization_callback) {
  VLOG(1) << "AccountManager::Initialize";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::TimeTicks initialization_start_time = base::TimeTicks::Now();

  if (init_state_ != InitializationState::kNotStarted) {
    // |Initialize| has already been called once. To help diagnose possible race
    // conditions, check whether the |home_dir| parameter provided by the first
    // invocation of |Initialize| matches the one it is currently being called
    // with.
    DCHECK_EQ(home_dir, home_dir_);
    RunOnInitialization(std::move(initialization_callback));
    return;
  }

  home_dir_ = home_dir;
  init_state_ = InitializationState::kInProgress;
  url_loader_factory_ = url_loader_factory;
  delay_network_call_runner_ = std::move(delay_network_call_runner);
  task_runner_ = task_runner;

  base::FilePath tokens_file_path;
  if (!IsEphemeralMode()) {
    DCHECK(task_runner_);
    tokens_file_path = home_dir_.Append(kTokensFileName);
    constexpr const char* kHistogramSuffix = "AccountManager";
    writer_ = std::make_unique<base::ImportantFileWriter>(
        tokens_file_path, task_runner_, kHistogramSuffix);
  }
  initialization_callbacks_.emplace_back(std::move(initialization_callback));

  if (!IsEphemeralMode()) {
    DCHECK(task_runner_);
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AccountManager::LoadAccountsFromDisk, tokens_file_path),
        base::BindOnce(
            &AccountManager::InsertAccountsAndRunInitializationCallbacks,
            weak_factory_.GetWeakPtr(), initialization_start_time));
  } else {
    // We are running in ephemeral mode. There is nothing to load from disk.
    RecordTokenLoadStatus(TokenLoadStatus::kSuccess);
    InsertAccountsAndRunInitializationCallbacks(initialization_start_time,
                                                /* accounts= */ AccountMap{});
  }
}

// static
AccountManager::AccountMap AccountManager::LoadAccountsFromDisk(
    const base::FilePath& tokens_file_path) {
  AccountMap accounts;

  VLOG(1) << "AccountManager::LoadTokensFromDisk";

  if (tokens_file_path.empty()) {
    RecordTokenLoadStatus(TokenLoadStatus::kSuccess);
    return accounts;
  }

  std::string token_file_data;
  bool success = base::ReadFileToStringWithMaxSize(
      tokens_file_path, &token_file_data, kTokensFileMaxSizeInBytes);
  if (!success) {
    if (base::PathExists(tokens_file_path)) {
      // The file exists but cannot be read from.
      LOG(ERROR) << "Unable to read accounts from disk";
      RecordTokenLoadStatus(TokenLoadStatus::kFileReadError);
    }
    return accounts;
  }

  internal::Accounts accounts_proto;
  success = accounts_proto.ParseFromString(token_file_data);
  if (!success) {
    LOG(ERROR) << "Failed to parse tokens from file";
    RecordTokenLoadStatus(TokenLoadStatus::kFileParseError);
    return accounts;
  }

  bool is_any_account_corrupt = false;
  for (const auto& account : accounts_proto.accounts()) {
    const std::optional<::account_manager::AccountType> account_type =
        FromProtoAccountType(account.account_type());
    if (!account_type.has_value()) {
      LOG(WARNING) << "Ignoring invalid account_type load from disk";
      is_any_account_corrupt = true;
      continue;
    }
    if (account.id().empty()) {
      LOG(WARNING) << "Ignoring invalid account_key load from disk";
      is_any_account_corrupt = true;
      continue;
    }
    ::account_manager::AccountKey account_key{account.id(),
                                              account_type.value()};
    accounts[account_key] = AccountInfo{account.raw_email(), account.token()};
  }
  if (is_any_account_corrupt) {
    RecordTokenLoadStatus(TokenLoadStatus::kAccountCorruptionDetected);
    return accounts;
  }

  RecordTokenLoadStatus(TokenLoadStatus::kSuccess);
  return accounts;
}

void AccountManager::InsertAccountsAndRunInitializationCallbacks(
    const base::TimeTicks& initialization_start_time,
    const AccountMap& accounts) {
  VLOG(1) << "AccountManager::RunInitializationCallbacks";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  accounts_.insert(accounts.begin(), accounts.end());
  init_state_ = InitializationState::kInitialized;
  RecordInitializationTime(initialization_start_time);

  for (auto& cb : initialization_callbacks_) {
    std::move(cb).Run();
  }
  initialization_callbacks_.clear();

  for (const auto& account : accounts_) {
    NotifyTokenObservers(::account_manager::Account{account.first /* key */,
                                                    account.second.raw_email});
  }

  RecordNumAccountsMetric(accounts_.size());
}

// AccountManager is supposed to be used as a leaky global.
AccountManager::~AccountManager() = default;

bool AccountManager::IsInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return init_state_ == InitializationState::kInitialized;
}

void AccountManager::RunOnInitialization(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (init_state_ != InitializationState::kInitialized) {
    initialization_callbacks_.emplace_back(std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

void AccountManager::GetAccounts(AccountListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure =
        base::BindOnce(&AccountManager::GetAccounts, weak_factory_.GetWeakPtr(),
                       std::move(callback));
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  std::move(callback).Run(GetAccountsView());
}

void AccountManager::GetAccountEmail(
    const ::account_manager::AccountKey& account_key,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure = base::BindOnce(
        &AccountManager::GetAccountEmail, weak_factory_.GetWeakPtr(),
        account_key, std::move(callback));
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    std::move(callback).Run(std::string());
    return;
  }

  std::move(callback).Run(it->second.raw_email);
}

void AccountManager::RemoveAccount(
    const ::account_manager::AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure =
        base::BindOnce(&AccountManager::RemoveAccount,
                       weak_factory_.GetWeakPtr(), account_key);
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    return;
  }

  const std::string raw_email = it->second.raw_email;
  const std::string old_token = it->second.token;
  accounts_.erase(it);
  PersistAccountsAsync();
  NotifyAccountRemovalObservers(
      ::account_manager::Account{account_key, raw_email});
  MaybeRevokeTokenOnServer(account_key, old_token);
}

void AccountManager::UpsertAccount(
    const ::account_manager::AccountKey& account_key,
    const std::string& raw_email,
    const std::string& token) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);
  DCHECK(!raw_email.empty());

  base::OnceClosure closure = base::BindOnce(
      &AccountManager::UpsertAccountInternal, weak_factory_.GetWeakPtr(),
      account_key, AccountInfo{raw_email, token});
  RunOnInitialization(std::move(closure));
}

void AccountManager::UpdateToken(
    const ::account_manager::AccountKey& account_key,
    const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (account_key.account_type() ==
      ::account_manager::AccountType::kActiveDirectory) {
    DCHECK_EQ(token, kActiveDirectoryDummyToken);
  }

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure =
        base::BindOnce(&AccountManager::UpdateToken, weak_factory_.GetWeakPtr(),
                       account_key, token);
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  auto it = accounts_.find(account_key);
  CHECK(it != accounts_.end(), base::NotFatalUntil::M130)
      << "UpdateToken cannot be used for adding accounts";
  UpsertAccountInternal(account_key, AccountInfo{it->second.raw_email, token});
}

void AccountManager::UpsertAccountInternal(
    const ::account_manager::AccountKey& account_key,
    const AccountInfo& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  if (account_key.account_type() == ::account_manager::AccountType::kGaia) {
    DCHECK(!account.raw_email.empty())
        << "Email must be present for Gaia accounts";
  }

  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    // This is a new account. Insert it.
    // Note: AccountManager may be used on Lacros in tests. Don't check pref
    // service in this case.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // New account insertions can only happen through a user action, which
    // implies that |Profile| must have been fully initialized at this point.
    // |ProfileImpl|'s constructor guarantees that
    // |AccountManager::SetPrefService| has been called on this object, which in
    // turn guarantees that |pref_service_| is not null.
    DCHECK(pref_service_);
    if (!pref_service_->GetBoolean(
            ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed)) {
      // Secondary Account additions are disabled by policy and all flows for
      // adding a Secondary Account are already blocked.
      CHECK(accounts_.empty());
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    accounts_.emplace(account_key, account);
    PersistAccountsAsync();
    NotifyTokenObservers(
        ::account_manager::Account{account_key, account.raw_email});
    return;
  }

  // Precondition: Iterator |it| is valid and points to a previously known
  // account.
  const std::string old_token = it->second.token;
  const bool did_token_change = (old_token != account.token);
  it->second = account;
  PersistAccountsAsync();

  if (did_token_change) {
    NotifyTokenObservers(
        ::account_manager::Account{account_key, account.raw_email});
  }
}

void AccountManager::PersistAccountsAsync() {
  if (IsEphemeralMode()) {
    return;
  }

  // Schedule (immediately) a non-blocking write.
  DCHECK(writer_);
  writer_->WriteNow(GetSerializedAccounts());
}

std::string AccountManager::GetSerializedAccounts() {
  internal::Accounts accounts_proto;

  for (const auto& account : accounts_) {
    internal::Account* account_proto = accounts_proto.add_accounts();
    account_proto->set_id(account.first.id());
    account_proto->set_account_type(
        ToProtoAccountType(account.first.account_type()));
    account_proto->set_raw_email(account.second.raw_email);
    account_proto->set_token(account.second.token);
  }

  return accounts_proto.SerializeAsString();
}

std::vector<::account_manager::Account> AccountManager::GetAccountsView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<::account_manager::Account> accounts;
  accounts.reserve(accounts_.size());

  for (const auto& key_val : accounts_) {
    accounts.emplace_back(
        ::account_manager::Account{key_val.first, key_val.second.raw_email});
  }

  return accounts;
}

void AccountManager::NotifyTokenObservers(
    const ::account_manager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnTokenUpserted(account);
  }
}

void AccountManager::NotifyAccountRemovalObservers(
    const ::account_manager::Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnAccountRemoved(account);
  }
}

void AccountManager::AddObserver(AccountManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void AccountManager::RemoveObserver(AccountManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void AccountManager::SetUrlLoaderFactoryForTests(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountManager::CreateAccessTokenFetcher(
    const ::account_manager::AccountKey& account_key,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto access_token_fetcher =
      std::make_unique<AccessTokenFetcher>(account_key, this, consumer);
  RunOnInitialization(access_token_fetcher->UnblockTokenRequest());
  return std::move(access_token_fetcher);
}

bool AccountManager::IsTokenAvailable(
    const ::account_manager::AccountKey& account_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = accounts_.find(account_key);
  return it != accounts_.end() && !it->second.token.empty() &&
         it->second.token != kActiveDirectoryDummyToken;
}

void AccountManager::HasDummyGaiaToken(
    const ::account_manager::AccountKey& account_key,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure = base::BindOnce(
        &AccountManager::HasDummyGaiaToken, weak_factory_.GetWeakPtr(),
        account_key, std::move(callback));
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  auto it = accounts_.find(account_key);
  std::move(callback).Run(it != accounts_.end() &&
                          it->second.token == kInvalidToken);
}

void AccountManager::CheckDummyGaiaTokenForAllAccounts(
    base::OnceCallback<
        void(const std::vector<std::pair<::account_manager::Account, bool>>&)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure =
        base::BindOnce(&AccountManager::CheckDummyGaiaTokenForAllAccounts,
                       weak_factory_.GetWeakPtr(), std::move(callback));
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  std::vector<std::pair<::account_manager::Account, bool>> accounts_list;
  accounts_list.reserve(accounts_.size());

  for (const auto& key_val : accounts_) {
    accounts_list.emplace_back(
        ::account_manager::Account{key_val.first, key_val.second.raw_email},
        key_val.second.token == kInvalidToken);
  }

  std::move(callback).Run(accounts_list);
}

void AccountManager::GetTokenHash(
    const ::account_manager::AccountKey& account_key,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (init_state_ != InitializationState::kInitialized) {
    base::OnceClosure closure = base::BindOnce(
        &AccountManager::GetTokenHash, weak_factory_.GetWeakPtr(), account_key,
        std::move(callback));
    RunOnInitialization(std::move(closure));
    return;
  }

  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    std::move(callback).Run(std::string());
    return;
  }

  std::move(callback).Run(Sha1Digest(it->second.token));
}

void AccountManager::MaybeRevokeTokenOnServer(
    const ::account_manager::AccountKey& account_key,
    const std::string& old_token) {
  if ((account_key.account_type() == ::account_manager::AccountType::kGaia) &&
      !old_token.empty() && (old_token != kInvalidToken)) {
    RevokeGaiaTokenOnServer(old_token);
  }
}

void AccountManager::RevokeGaiaTokenOnServer(const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_token_revocation_requests_.emplace_back(
      std::make_unique<GaiaTokenRevocationRequest>(
          url_loader_factory_, delay_network_call_runner_, refresh_token,
          weak_factory_.GetWeakPtr()));
}

void AccountManager::DeletePendingTokenRevocationRequest(
    GaiaTokenRevocationRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it =
      base::ranges::find(pending_token_revocation_requests_, request,
                         &std::unique_ptr<GaiaTokenRevocationRequest>::get);

  if (it != pending_token_revocation_requests_.end()) {
    pending_token_revocation_requests_.erase(it);
  }
}

bool AccountManager::IsEphemeralMode() const {
  return home_dir_.empty();
}

std::optional<std::string> AccountManager::GetRefreshToken(
    const ::account_manager::AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  DCHECK(account_key.account_type() == ::account_manager::AccountType::kGaia);

  auto it = accounts_.find(account_key);
  if (it == accounts_.end() || it->second.token.empty()) {
    return std::nullopt;
  }

  return std::make_optional<std::string>(it->second.token);
}

scoped_refptr<network::SharedURLLoaderFactory>
AccountManager::GetUrlLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  return url_loader_factory_;
}

base::WeakPtr<AccountManager> AccountManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace account_manager
