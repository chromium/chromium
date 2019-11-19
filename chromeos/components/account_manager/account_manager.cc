// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/account_manager/account_manager.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace chromeos {

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

  ~GaiaTokenRevocationRequest() override = default;

  // GaiaAuthConsumer overrides.
  void OnOAuth2RevokeTokenCompleted(TokenRevocationStatus status) override {
    VLOG(1) << "GaiaTokenRevocationRequest::OnOAuth2RevokeTokenCompleted";
    // We cannot call |AccountManager::DeletePendingTokenRevocationRequest|
    // directly because it will immediately start deleting |this|, before the
    // method has had a chance to return.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
  DISALLOW_COPY_AND_ASSIGN(GaiaTokenRevocationRequest);
};

bool AccountManager::AccountKey::IsValid() const {
  return !id.empty() &&
         account_type != account_manager::AccountType::ACCOUNT_TYPE_UNSPECIFIED;
}

bool AccountManager::AccountKey::operator<(const AccountKey& other) const {
  if (id != other.id) {
    return id < other.id;
  }

  return account_type < other.account_type;
}

bool AccountManager::AccountKey::operator==(const AccountKey& other) const {
  return id == other.id && account_type == other.account_type;
}

bool AccountManager::AccountKey::operator!=(const AccountKey& other) const {
  return !(*this == other);
}

AccountManager::Observer::Observer() = default;

AccountManager::Observer::~Observer() = default;

AccountManager::AccountManager() {}

// static
void AccountManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      chromeos::prefs::kSecondaryGoogleAccountSigninAllowed,
      true /* default_value */);
}

void AccountManager::SetPrefService(PrefService* pref_service) {
  DCHECK(pref_service);
  pref_service_ = pref_service;
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
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
           base::MayBlock()}),
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
    DCHECK_EQ(home_dir, writer_->path().DirName());
    std::move(initialization_callback).Run();
    return;
  }

  init_state_ = InitializationState::kInProgress;
  url_loader_factory_ = url_loader_factory;
  delay_network_call_runner_ = std::move(delay_network_call_runner);
  task_runner_ = task_runner;
  writer_ = std::make_unique<base::ImportantFileWriter>(
      home_dir.Append(kTokensFileName), task_runner_);
  initialization_callbacks_.emplace_back(std::move(initialization_callback));

  PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&AccountManager::LoadAccountsFromDisk, writer_->path()),
      base::BindOnce(
          &AccountManager::InsertAccountsAndRunInitializationCallbacks,
          weak_factory_.GetWeakPtr(), initialization_start_time));
}

// static
AccountManager::AccountMap AccountManager::LoadAccountsFromDisk(
    const base::FilePath& tokens_file_path) {
  AccountManager::AccountMap accounts;

  VLOG(1) << "AccountManager::LoadTokensFromDisk";
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

  chromeos::account_manager::Accounts accounts_proto;
  success = accounts_proto.ParseFromString(token_file_data);
  if (!success) {
    LOG(ERROR) << "Failed to parse tokens from file";
    RecordTokenLoadStatus(TokenLoadStatus::kFileParseError);
    return accounts;
  }

  bool is_any_account_corrupt = false;
  for (const auto& account : accounts_proto.accounts()) {
    AccountManager::AccountKey account_key{account.id(),
                                           account.account_type()};

    if (!account_key.IsValid()) {
      LOG(WARNING) << "Ignoring invalid account_key load from disk: "
                   << account_key;
      is_any_account_corrupt = true;
      continue;
    }
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
    NotifyTokenObservers(
        Account{account.first /* key */, account.second.raw_email});
  }

  RecordNumAccountsMetric(accounts_.size());
}

AccountManager::~AccountManager() {
  // AccountManager is supposed to be used as a leaky global.
}

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
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::GetAccountsInternal,
                     weak_factory_.GetWeakPtr(), std::move(callback));
  RunOnInitialization(std::move(closure));
}

void AccountManager::GetAccountsInternal(AccountListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  std::move(callback).Run(GetAccounts());
}

void AccountManager::GetAccountEmail(
    const AccountKey& account_key,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure = base::BindOnce(
      &AccountManager::GetAccountEmailInternal, weak_factory_.GetWeakPtr(),
      account_key, std::move(callback));
  RunOnInitialization(std::move(closure));
}

void AccountManager::GetAccountEmailInternal(
    const AccountKey& account_key,
    base::OnceCallback<void(const std::string&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    std::move(callback).Run(std::string());
  }

  std::move(callback).Run(it->second.raw_email);
}

void AccountManager::RemoveAccount(const AccountKey& account_key) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::RemoveAccountInternal,
                     weak_factory_.GetWeakPtr(), account_key);
  RunOnInitialization(std::move(closure));
}

void AccountManager::RemoveAccountInternal(const AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    return;
  }

  const std::string raw_email = it->second.raw_email;
  const std::string old_token = it->second.token;
  accounts_.erase(it);
  PersistAccountsAsync();
  NotifyAccountRemovalObservers(Account{account_key, raw_email});
  MaybeRevokeTokenOnServer(account_key, old_token);
}

void AccountManager::RemoveAccount(const std::string& email) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::RemoveAccountByEmailInternal,
                     weak_factory_.GetWeakPtr(), email);
  RunOnInitialization(std::move(closure));
}

void AccountManager::RemoveAccountByEmailInternal(const std::string& email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  for (const std::pair<AccountKey, AccountInfo> account : accounts_) {
    if (gaia::AreEmailsSame(account.second.raw_email, email)) {
      RemoveAccountInternal(account.first /* account_key */);
      return;
    }
  }
}

void AccountManager::UpsertAccount(const AccountKey& account_key,
                                   const std::string& raw_email,
                                   const std::string& token) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);
  DCHECK(!raw_email.empty());

  base::OnceClosure closure = base::BindOnce(
      &AccountManager::UpsertAccountInternal, weak_factory_.GetWeakPtr(),
      account_key, AccountInfo{raw_email, token});
  RunOnInitialization(std::move(closure));
}

void AccountManager::UpdateToken(const AccountKey& account_key,
                                 const std::string& token) {
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  if (account_key.account_type ==
      account_manager::AccountType::ACCOUNT_TYPE_ACTIVE_DIRECTORY) {
    DCHECK_EQ(token, kActiveDirectoryDummyToken);
  }

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::UpdateTokenInternal,
                     weak_factory_.GetWeakPtr(), account_key, token);
  RunOnInitialization(std::move(closure));
}

void AccountManager::UpdateTokenInternal(const AccountKey& account_key,
                                         const std::string& token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  auto it = accounts_.find(account_key);
  DCHECK(it != accounts_.end())
      << "UpdateToken cannot be used for adding accounts";
  UpsertAccountInternal(account_key, AccountInfo{it->second.raw_email, token});
}

void AccountManager::UpdateEmail(const AccountKey& account_key,
                                 const std::string& raw_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(init_state_, InitializationState::kNotStarted);

  base::OnceClosure closure =
      base::BindOnce(&AccountManager::UpdateEmailInternal,
                     weak_factory_.GetWeakPtr(), account_key, raw_email);
  RunOnInitialization(std::move(closure));
}

void AccountManager::UpdateEmailInternal(const AccountKey& account_key,
                                         const std::string& raw_email) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  auto it = accounts_.find(account_key);
  DCHECK(it != accounts_.end())
      << "UpdateEmail cannot be used for adding accounts";
  UpsertAccountInternal(account_key, AccountInfo{raw_email, it->second.token});
}

void AccountManager::UpsertAccountInternal(const AccountKey& account_key,
                                           const AccountInfo& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);
  DCHECK(account_key.IsValid()) << "Invalid account_key: " << account_key;

  if (account_key.account_type ==
      account_manager::AccountType::ACCOUNT_TYPE_GAIA) {
    DCHECK(!account.raw_email.empty())
        << "Email must be present for Gaia accounts";
  }

  auto it = accounts_.find(account_key);
  if (it == accounts_.end()) {
    // This is a new account. Insert it.

    // New account insertions can only happen through a user action, which
    // implies that |Profile| must have been fully initialized at this point.
    // |ProfileImpl|'s constructor guarantees that
    // |AccountManager::SetPrefService| has been called on this object, which in
    // turn guarantees that |pref_service_| is not null.
    DCHECK(pref_service_);
    if (!pref_service_->GetBoolean(
            chromeos::prefs::kSecondaryGoogleAccountSigninAllowed)) {
      // Secondary Account additions are disabled by policy and all flows for
      // adding a Secondary Account are already blocked.
      CHECK(accounts_.empty());
    }
    accounts_.emplace(account_key, account);
    PersistAccountsAsync();
    NotifyTokenObservers(Account{account_key, account.raw_email});
    return;
  }

  // Precondition: Iterator |it| is valid and points to a previously known
  // account.
  const std::string old_token = it->second.token;
  const bool did_token_change = (old_token != account.token);
  it->second = account;
  PersistAccountsAsync();

  if (did_token_change) {
    NotifyTokenObservers(Account{account_key, account.raw_email});
  }
}

void AccountManager::PersistAccountsAsync() {
  // Schedule (immediately) a non-blocking write.
  writer_->WriteNow(std::make_unique<std::string>(GetSerializedAccounts()));
}

std::string AccountManager::GetSerializedAccounts() {
  chromeos::account_manager::Accounts accounts_proto;

  for (const auto& account : accounts_) {
    account_manager::Account* account_proto = accounts_proto.add_accounts();
    account_proto->set_id(account.first.id);
    account_proto->set_account_type(account.first.account_type);
    account_proto->set_raw_email(account.second.raw_email);
    account_proto->set_token(account.second.token);
  }

  return accounts_proto.SerializeAsString();
}

std::vector<AccountManager::Account> AccountManager::GetAccounts() {
  std::vector<Account> accounts;
  accounts.reserve(accounts_.size());

  for (const auto& key_val : accounts_) {
    accounts.emplace_back(Account{key_val.first, key_val.second.raw_email});
  }

  return accounts;
}

void AccountManager::NotifyTokenObservers(const Account& account) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : observers_) {
    observer.OnTokenUpserted(account);
  }
}

void AccountManager::NotifyAccountRemovalObservers(const Account& account) {
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

scoped_refptr<network::SharedURLLoaderFactory>
AccountManager::GetUrlLoaderFactory() {
  DCHECK(url_loader_factory_);
  return url_loader_factory_;
}

void AccountManager::SetUrlLoaderFactoryForTests(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = url_loader_factory;
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountManager::CreateAccessTokenFetcher(
    const AccountKey& account_key,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = accounts_.find(account_key);
  if (it == accounts_.end() || it->second.token.empty()) {
    return nullptr;
  }

  return std::make_unique<OAuth2AccessTokenFetcherImpl>(
      consumer, url_loader_factory, it->second.token);
}

bool AccountManager::IsTokenAvailable(const AccountKey& account_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = accounts_.find(account_key);
  return it != accounts_.end() && !it->second.token.empty() &&
         it->second.token != kActiveDirectoryDummyToken;
}

bool AccountManager::HasDummyGaiaToken(const AccountKey& account_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(init_state_, InitializationState::kInitialized);

  auto it = accounts_.find(account_key);
  return it != accounts_.end() && it->second.token == kInvalidToken;
}

void AccountManager::MaybeRevokeTokenOnServer(const AccountKey& account_key,
                                              const std::string& old_token) {
  if ((account_key.account_type ==
       account_manager::AccountType::ACCOUNT_TYPE_GAIA) &&
      !old_token.empty() && (old_token != kInvalidToken)) {
    RevokeGaiaTokenOnServer(old_token);
  }
}

void AccountManager::RevokeGaiaTokenOnServer(const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_token_revocation_requests_.emplace_back(
      std::make_unique<GaiaTokenRevocationRequest>(
          GetUrlLoaderFactory(), delay_network_call_runner_, refresh_token,
          weak_factory_.GetWeakPtr()));
}

void AccountManager::DeletePendingTokenRevocationRequest(
    GaiaTokenRevocationRequest* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = std::find_if(
      pending_token_revocation_requests_.begin(),
      pending_token_revocation_requests_.end(),
      [&request](
          const std::unique_ptr<GaiaTokenRevocationRequest>& pending_request)
          -> bool { return pending_request.get() == request; });

  if (it != pending_token_revocation_requests_.end()) {
    pending_token_revocation_requests_.erase(it);
  }
}

COMPONENT_EXPORT(ACCOUNT_MANAGER)
std::ostream& operator<<(std::ostream& os,
                         const AccountManager::AccountKey& account_key) {
  os << "{ id: " << account_key.id
     << ", account_type: " << account_key.account_type << " }";

  return os;
}

}  // namespace chromeos
