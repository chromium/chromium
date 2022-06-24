// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash {

namespace {

// Specialized structs for each auth factor with factor-specific metadata.
// Secrets are stored the same way they are sent to cryptohome (i.e. salted and
// hashed), but only if secret checking has been enabled via
// `TestApi::set_enabled_auth_check`.
// `FakeAuthFactor` is the union/absl::variant of the factor-specific auth
// factor structs.

struct PasswordFactor {
  // This will be `absl::nullopt` if auth checking hasn't been activated.
  absl::optional<std::string> password;
};

struct PinFactor {
  // This will be `absl::nullopt` if auth checking hasn't been activated.
  absl::optional<std::string> pin = absl::nullopt;
  bool locked = false;
};

struct RecoveryFactor {};

struct KioskFactor {};

using FakeAuthFactor =
    absl::variant<PasswordFactor, PinFactor, RecoveryFactor, KioskFactor>;

}  // namespace

struct FakeUserDataAuthClient::UserCryptohomeState {
  // Maps labels to auth factors.
  std::map<std::string, FakeAuthFactor> auth_factors;
};

namespace {

// Interval to update the progress of MigrateToDircrypto in milliseconds.
constexpr int kDircryptoMigrationUpdateIntervalMs = 200;
// The number of updates the MigrateToDircrypto will send before it completes.
constexpr uint64_t kDircryptoMigrationMaxProgress = 15;

// Template for auth session ID.
constexpr char kAuthSessionIdTemplate[] = "AuthSession-%d";

// Guest username constant that mirrors the one in real cryptohome
constexpr char kGuestUserName[] = "$guest";

// Used to track the fake instance, mirrors the instance in the base class.
FakeUserDataAuthClient* g_instance = nullptr;

// Used to construct a functor/visitor with overloaded `operator()` from a list
// of lambdas. Useful for visiting absl::variant.
template <class... Ts>
struct OverloadedFunctor : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
OverloadedFunctor<Ts...> Overload(Ts&&... ts) {
  return OverloadedFunctor<Ts...>{std::forward<Ts>(ts)...};
}

// Helper that automatically sends a reply struct to a supplied callback when
// it goes out of scope. Basically a specialized `absl::Cleanup` or
// `std::scope_exit`.
template <typename ReplyType>
class ReplyOnReturn {
 public:
  explicit ReplyOnReturn(ReplyType* reply,
                         DBusMethodCallback<ReplyType> callback)
      : reply_(reply), callback_(std::move(callback)) {}
  ReplyOnReturn(const ReplyOnReturn<ReplyType>&) = delete;

  ~ReplyOnReturn() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), *reply_));
  }

  ReplyOnReturn<ReplyType>& operator=(const ReplyOnReturn<ReplyType>&) = delete;

 private:
  raw_ptr<ReplyType> reply_;
  DBusMethodCallback<ReplyType> callback_;
};

}  // namespace

// Allocate space for test api instance
base::raw_ptr<FakeUserDataAuthClient::TestApi>
    FakeUserDataAuthClient::TestApi::instance_;

FakeUserDataAuthClient::TestApi::TestApi(
    base::raw_ptr<FakeUserDataAuthClient> client) {
  DCHECK(client != nullptr);
  client_ = client;
}

// static
FakeUserDataAuthClient::TestApi* FakeUserDataAuthClient::TestApi::Get() {
  if (instance_ == nullptr) {
    instance_ = new TestApi(FakeUserDataAuthClient::Get());
  }
  return instance_;
}

void FakeUserDataAuthClient::TestApi::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;
  client_->RunPendingWaitForServiceToBeAvailableCallbacks();
}

void FakeUserDataAuthClient::TestApi::ReportServiceIsNotAvailable() {
  DCHECK(!service_is_available_);
  service_reported_not_available_ = true;
  client_->RunPendingWaitForServiceToBeAvailableCallbacks();
}

void FakeUserDataAuthClient::TestApi::SetEcryptfsUserHome(
    const cryptohome::AccountIdentifier& cryptohome_id,
    bool use_ecryptfs) {
  client_->SetEcryptfsUserHome(cryptohome_id, use_ecryptfs);
}

void FakeUserDataAuthClient::TestApi::SetPinLocked(
    const cryptohome::AccountIdentifier& account_id,
    const std::string& label,
    bool locked) {
  auto user_it = client_->users_.find(account_id);
  CHECK(user_it != client_->users_.end())
      << "User does not exist: " << account_id.account_id();
  UserCryptohomeState& user_state = user_it->second;

  auto factor_it = user_state.auth_factors.find(label);
  CHECK(factor_it != user_state.auth_factors.end())
      << "Factor does not exist: " << label;
  FakeAuthFactor& factor = factor_it->second;

  PinFactor* pin_factor = absl::get_if<PinFactor>(&factor);
  CHECK(pin_factor) << "Factor is not PIN: " << label;

  pin_factor->locked = locked;
}

FakeUserDataAuthClient::FakeUserDataAuthClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeUserDataAuthClient::~FakeUserDataAuthClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FakeUserDataAuthClient* FakeUserDataAuthClient::Get() {
  return g_instance;
}

void FakeUserDataAuthClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeUserDataAuthClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeUserDataAuthClient::IsMounted(
    const ::user_data_auth::IsMountedRequest& request,
    IsMountedCallback callback) {
  ::user_data_auth::IsMountedReply reply;
  reply.set_is_mounted(true);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::Unmount(
    const ::user_data_auth::UnmountRequest& request,
    UnmountCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::UnmountReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::Mount(
    const ::user_data_auth::MountRequest& request,
    MountCallback callback) {
  ::user_data_auth::CryptohomeErrorCode error = cryptohome_error_;
  last_mount_request_ = request;
  ++mount_request_count_;
  ::user_data_auth::MountReply reply;

  cryptohome::AccountIdentifier account;
  if (request.guest_mount()) {
    account.set_account_id(kGuestUserName);
    reply.set_sanitized_username(GetStubSanitizedUsername(account));
  } else {
    if (request.has_account()) {
      account = request.account();
      reply.set_sanitized_username(GetStubSanitizedUsername(account));
      if (TestApi::Get()->mount_create_required_ && !request.has_create())
        error = ::user_data_auth::CryptohomeErrorCode::
            CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND;
    } else {
      auto auth_session = auth_sessions_.find(request.auth_session_id());
      DCHECK(auth_session != std::end(auth_sessions_));
      account = auth_session->second.account;
    }

    reply.set_sanitized_username(GetStubSanitizedUsername(account));
    if (IsEcryptfsUserHome(account) && !request.to_migrate_from_ecryptfs() &&
        request.force_dircrypto_if_available()) {
      error = ::user_data_auth::CryptohomeErrorCode::
          CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION;
    }
  }

  reply.set_error(error);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::Remove(
    const ::user_data_auth::RemoveRequest& request,
    RemoveCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::RemoveReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::GetKeyData(
    const ::user_data_auth::GetKeyDataRequest& request,
    GetKeyDataCallback callback) {
  ::user_data_auth::GetKeyDataReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  // Check if user exists.
  const auto user_it = users_.find(request.account_id());
  if (user_it == std::end(users_)) {
    LOG(ERROR) << "User does not exist: " << request.account_id().account_id();
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }
  const UserCryptohomeState& user_state = user_it->second;

  const std::string& requested_label =
      request.authorization_request().key().data().label();

  // Create range [factors_begin, factors_end) of factors matching
  // `requested_label`: If the `requested_label` is empty, then every factor
  // matches. Otherwise the factor with that precise label matches. If no such
  // factor exists, the range is empty.
  auto factors_begin = std::begin(user_state.auth_factors);
  auto factors_end = std::end(user_state.auth_factors);
  if (!requested_label.empty()) {
    factors_begin = user_state.auth_factors.find(requested_label);
    if (factors_begin != factors_end) {
      factors_end = std::next(factors_begin);
    }
  }

  // Fill `reply.key_data()` with the factors we found.
  for (auto factors_it = factors_begin; factors_it != factors_end;
       ++factors_it) {
    const std::string& label = factors_it->first;
    const FakeAuthFactor& factor = factors_it->second;

    absl::visit(Overload(
                    [&](const PasswordFactor& password) {
                      cryptohome::KeyData* data = reply.add_key_data();
                      data->set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
                      data->set_label(label);
                    },
                    [&](const PinFactor& pin) {
                      cryptohome::KeyData* data = reply.add_key_data();
                      data->set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
                      data->set_label(label);
                      data->mutable_policy()->set_low_entropy_credential(true);
                      data->mutable_policy()->set_auth_locked(pin.locked);
                    },
                    [&](const RecoveryFactor& recovery) {
                      // Retrieving key data for recovery code via the legacy
                      // API is not allowed.
                      LOG(WARNING) << "Not reporting recovery key: " << label;
                    },
                    [&](const KioskFactor& kiosk) {
                      cryptohome::KeyData* data = reply.add_key_data();
                      data->set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
                      data->set_label(label);
                    }),
                factor);
  }

  if (reply.key_data().empty()) {
    // This happens if no or only unsupported factors matched the request.
    LOG(ERROR) << "No legacy key exists for label " << requested_label;
    reply.set_error(
        ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  }
}
void FakeUserDataAuthClient::CheckKey(
    const ::user_data_auth::CheckKeyRequest& request,
    CheckKeyCallback callback) {
  ::user_data_auth::CheckKeyReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (!TestApi::Get()->enable_auth_check_) {
    return;
  }

  last_unlock_webauthn_secret_ = request.unlock_webauthn_secret();

  const auto user_it = users_.find(request.account_id());
  if (user_it == std::end(users_)) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }
  const UserCryptohomeState& user_state = user_it->second;

  const cryptohome::Key& key = request.authorization_request().key();
  const std::string& label = key.data().label();

  const auto factor_it = user_state.auth_factors.find(label);
  if (factor_it == std::end(user_state.auth_factors)) {
    // Factor does not exist.
    reply.set_error(
        ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    return;
  }
  const FakeAuthFactor& factor = factor_it->second;

  absl::visit(
      Overload(
          [&](const PasswordFactor& password) {
            const std::string& secret = key.secret();
            if (password.password != secret) {
              reply.set_error(::user_data_auth::CryptohomeErrorCode::
                                  CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
            }
          },
          [&](const PinFactor& pin) {
            const std::string& secret = key.secret();
            if (pin.pin != secret) {
              reply.set_error(::user_data_auth::CryptohomeErrorCode::
                                  CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
            }
          },
          [&](const RecoveryFactor& recovery) {
            LOG(FATAL) << "Checking recovery key is not allowed";
          },
          [&](const KioskFactor& kiosk) {
            // Kiosk key secrets are derived from app ids and don't leave
            // cryptohome, so there's nothing to check.
          }),
      factor);
}
void FakeUserDataAuthClient::AddKey(
    const ::user_data_auth::AddKeyRequest& request,
    AddKeyCallback callback) {
  ::user_data_auth::AddKeyReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const cryptohome::AccountIdentifier& account_id = request.account_id();
  const bool clobber_if_exists = request.clobber_if_exists();
  const cryptohome::Key new_key = request.key();
  const cryptohome::KeyData new_key_data = new_key.data();
  const std::string& new_label = new_key_data.label();
  CHECK_NE(new_label, "") << "Label must not be empty string";

  auto user_it = users_.find(account_id);
  if (user_it == std::end(users_)) {
    // TODO(crbug.com/1334538): Cryptohome would not create a new user here,
    // but many tests rely on it. New tests shouldn't rely on this behavior.
    LOG(ERROR) << "Need to create new user: " << account_id.account_id();
    user_it = users_.insert(user_it, {account_id, UserCryptohomeState()});
  }
  DCHECK(user_it != std::end(users_));
  std::map<std::string, FakeAuthFactor>& auth_factors =
      user_it->second.auth_factors;

  if (!clobber_if_exists) {
    CHECK(auth_factors.find(new_label) == std::end(auth_factors))
        << "Key exists, will not clobber: " << new_label;
  }

  // We only save the secret if auth checking is enabled.
  absl::optional<std::string> secret =
      TestApi::Get()->enable_auth_check_
          ? absl::optional<std::string>(new_key.secret())
          : absl::nullopt;

  const cryptohome::KeyData::KeyType new_key_type = new_key_data.type();

  if (new_key_type == cryptohome::KeyData::KEY_TYPE_KIOSK) {
    auth_factors[new_label] = KioskFactor{};
    return;
  }

  LOG_IF(WARNING, new_key_type != cryptohome::KeyData::KEY_TYPE_PASSWORD)
      << "Unsupported key type, assuming KEY_TYPE_PASSWORD: " << new_key_type;

  const bool is_low_entropy =
      new_key_data.has_policy() &&
      new_key_data.policy().has_low_entropy_credential() &&
      new_key_data.policy().low_entropy_credential();

  if (is_low_entropy) {
    auth_factors[new_label] = PinFactor{std::move(secret)};
    return;
  }

  auth_factors[new_label] = PasswordFactor{std::move(secret)};
}
void FakeUserDataAuthClient::RemoveKey(
    const ::user_data_auth::RemoveKeyRequest& request,
    RemoveKeyCallback callback) {
  ::user_data_auth::RemoveKeyReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const auto user_it = users_.find(request.account_id());
  if (user_it == std::end(users_)) {
    // TODO(crbug.com/1334538): Cryptohome would report an error here, but many
    // tests do not set up users before calling RemoveKey. That's why we don't
    // report an error here. New tests shouldn't rely on this behavior.
    LOG(ERROR) << "User does not exist: " << request.account_id().account_id();
    return;
  }
  UserCryptohomeState& user_state = user_it->second;

  const std::string& label = request.key().data().label();
  if (label.empty()) {
    // An empty request label matches all keys, so remove all.
    LOG(WARNING) << "RemoveKey for empty label removes all keys";
    user_state.auth_factors.clear();
  } else {
    user_state.auth_factors.erase(label);
  }
}
void FakeUserDataAuthClient::MassRemoveKeys(
    const ::user_data_auth::MassRemoveKeysRequest& request,
    MassRemoveKeysCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::MassRemoveKeysReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::MigrateKey(
    const ::user_data_auth::MigrateKeyRequest& request,
    MigrateKeyCallback callback) {
  ReturnProtobufMethodCallback(::user_data_auth::MigrateKeyReply(),
                               std::move(callback));
}
void FakeUserDataAuthClient::StartFingerprintAuthSession(
    const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
    StartFingerprintAuthSessionCallback callback) {
  ReturnProtobufMethodCallback(
      ::user_data_auth::StartFingerprintAuthSessionReply(),
      std::move(callback));
}
void FakeUserDataAuthClient::EndFingerprintAuthSession(
    const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
    EndFingerprintAuthSessionCallback callback) {
  ReturnProtobufMethodCallback(
      ::user_data_auth::EndFingerprintAuthSessionReply(), std::move(callback));
}
void FakeUserDataAuthClient::StartMigrateToDircrypto(
    const ::user_data_auth::StartMigrateToDircryptoRequest& request,
    StartMigrateToDircryptoCallback callback) {
  last_migrate_to_dircrypto_request_ = request;
  ReturnProtobufMethodCallback(::user_data_auth::StartMigrateToDircryptoReply(),
                               std::move(callback));

  dircrypto_migration_progress_ = 0;

  if (TestApi::Get()->run_default_dircrypto_migration_) {
    dircrypto_migration_progress_timer_.Start(
        FROM_HERE, base::Milliseconds(kDircryptoMigrationUpdateIntervalMs),
        this, &FakeUserDataAuthClient::OnDircryptoMigrationProgressUpdated);
  }
}
void FakeUserDataAuthClient::NeedsDircryptoMigration(
    const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
    NeedsDircryptoMigrationCallback callback) {
  ::user_data_auth::NeedsDircryptoMigrationReply reply;
  reply.set_needs_dircrypto_migration(IsEcryptfsUserHome(request.account_id()));
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::GetSupportedKeyPolicies(
    const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
    GetSupportedKeyPoliciesCallback callback) {
  ::user_data_auth::GetSupportedKeyPoliciesReply reply;
  reply.set_low_entropy_credentials_supported(
      TestApi::Get()->supports_low_entropy_credentials_);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::GetAccountDiskUsage(
    const ::user_data_auth::GetAccountDiskUsageRequest& request,
    GetAccountDiskUsageCallback callback) {
  ::user_data_auth::GetAccountDiskUsageReply reply;
  // Sets 100 MB as a fake usage.
  reply.set_size(100 * 1024 * 1024);
  ReturnProtobufMethodCallback(reply, std::move(callback));
}
void FakeUserDataAuthClient::StartAuthSession(
    const ::user_data_auth::StartAuthSessionRequest& request,
    StartAuthSessionCallback callback) {
  std::string auth_session_id =
      base::StringPrintf(kAuthSessionIdTemplate, next_auth_session_id_++);

  DCHECK_EQ(auth_sessions_.count(auth_session_id), 0u);
  AuthSessionData& session = auth_sessions_[auth_session_id];
  session.id = auth_session_id;
  session.account = request.account_id();

  std::string user_id = request.account_id().account_id();
  // See device_local_account.h
  bool is_kiosk = base::EndsWith(user_id, "kiosk-apps.device-local.localhost");

  ::user_data_auth::StartAuthSessionReply reply;
  if (cryptohome_error_ !=
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(cryptohome_error_);
  } else {
    reply.set_auth_session_id(auth_session_id);
    bool user_exists = UserExists(request.account_id());
    reply.set_user_exists(user_exists);
    if (user_exists) {
      if (is_kiosk) {
        // see kCryptohomePublicMountLabel
        std::string kiosk_label = "publicmount";
        cryptohome::KeyData kiosk_key;
        kiosk_key.set_label(kiosk_label);
        kiosk_key.set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
        (*reply.mutable_key_label_data())[kiosk_label] = std::move(kiosk_key);
      } else {
        // see kCryptohomeGaiaKeyLabel
        std::string gaia_label = "gaia";
        cryptohome::KeyData gaia_key;
        gaia_key.set_label(gaia_label);
        gaia_key.set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
        (*reply.mutable_key_label_data())[gaia_label] = std::move(gaia_key);
      }
    }
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::AuthenticateAuthSession(
    const ::user_data_auth::AuthenticateAuthSessionRequest& request,
    AuthenticateAuthSessionCallback callback) {
  last_authenticate_auth_session_request_ = request;
  ::user_data_auth::AuthenticateAuthSessionReply reply;

  const std::string auth_session_id = request.auth_session_id();

  const auto it = auth_sessions_.find(auth_session_id);
  if (it == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else if (cryptohome_error_ !=
             ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(cryptohome_error_);
  } else {
    it->second.authenticated = true;
    reply.set_authenticated(true);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::AddCredentials(
    const ::user_data_auth::AddCredentialsRequest& request,
    AddCredentialsCallback callback) {
  last_add_credentials_request_ = request;
  ::user_data_auth::AddCredentialsReply reply;

  const std::string auth_session_id = request.auth_session_id();

  const auto it = auth_sessions_.find(auth_session_id);
  if (it == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::UpdateCredential(
    const ::user_data_auth::UpdateCredentialRequest& request,
    UpdateCredentialCallback callback) {
  ::user_data_auth::UpdateCredentialReply reply;

  const std::string auth_session_id = request.auth_session_id();

  const auto it = auth_sessions_.find(auth_session_id);
  if (it == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else if (!it->second.authenticated) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_UNAUTHENTICATED_AUTH_SESSION);
  } else {
    reply.set_error(cryptohome_error_);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::PrepareGuestVault(
    const ::user_data_auth::PrepareGuestVaultRequest& request,
    PrepareGuestVaultCallback callback) {
  ::user_data_auth::PrepareGuestVaultReply reply;
  prepare_guest_request_count_++;

  cryptohome::AccountIdentifier account;
  account.set_account_id(kGuestUserName);
  reply.set_sanitized_username(GetStubSanitizedUsername(account));

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::PrepareEphemeralVault(
    const ::user_data_auth::PrepareEphemeralVaultRequest& request,
    PrepareEphemeralVaultCallback callback) {
  ::user_data_auth::PrepareEphemeralVaultReply reply;

  cryptohome::AccountIdentifier account;
  auto auth_session = auth_sessions_.find(request.auth_session_id());
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else {
    account = auth_session->second.account;
    // Ephemeral mount does not require session to be authenticated;
    // It authenticates session instead.
    if (auth_session->second.authenticated) {
      LOG(ERROR) << "AuthSession is authenticated";
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    } else {
      auth_session->second.authenticated = true;
    }

    reply.set_sanitized_username(GetStubSanitizedUsername(account));
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::CreatePersistentUser(
    const ::user_data_auth::CreatePersistentUserRequest& request,
    CreatePersistentUserCallback callback) {
  ::user_data_auth::CreatePersistentUserReply reply;

  auto auth_session = auth_sessions_.find(request.auth_session_id());
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else if (UserExists(auth_session->second.account)) {
    LOG(ERROR) << "User already exists"
               << GetStubSanitizedUsername(auth_session->second.account);
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
  } else {
    auth_session->second.authenticated = true;
    AddExistingUser(auth_session->second.account);
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::PreparePersistentVault(
    const ::user_data_auth::PreparePersistentVaultRequest& request,
    PreparePersistentVaultCallback callback) {
  ::user_data_auth::PreparePersistentVaultReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    reply.set_error(error);
  } else if (!UserExists(authenticated_auth_session->account)) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
  } else {
    reply.set_sanitized_username(
        GetStubSanitizedUsername(authenticated_auth_session->account));
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::PrepareVaultForMigration(
    const ::user_data_auth::PrepareVaultForMigrationRequest& request,
    PrepareVaultForMigrationCallback callback) {
  ::user_data_auth::PrepareVaultForMigrationReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    reply.set_error(error);
  } else if (!UserExists(authenticated_auth_session->account)) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
  }

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::InvalidateAuthSession(
    const ::user_data_auth::InvalidateAuthSessionRequest& request,
    InvalidateAuthSessionCallback callback) {
  ::user_data_auth::InvalidateAuthSessionReply reply;
  auto auth_session = auth_sessions_.find(request.auth_session_id());
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else {
    auth_sessions_.erase(auth_session);
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::ExtendAuthSession(
    const ::user_data_auth::ExtendAuthSessionRequest& request,
    ExtendAuthSessionCallback callback) {
  ::user_data_auth::ExtendAuthSessionReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::AddAuthFactor(
    const ::user_data_auth::AddAuthFactorRequest& request,
    AddAuthFactorCallback callback) {
  ::user_data_auth::AddAuthFactorReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::AuthenticateAuthFactor(
    const ::user_data_auth::AuthenticateAuthFactorRequest& request,
    AuthenticateAuthFactorCallback callback) {
  last_authenticate_auth_factor_request_ = request;
  ::user_data_auth::AuthenticateAuthFactorReply reply;

  const std::string auth_session_id = request.auth_session_id();
  const auto auth_session = auth_sessions_.find(auth_session_id);
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else if (auth_session->second.authenticated) {
    LOG(WARNING) << "AuthSession is already authenticated";
  } else {
    auth_session->second.authenticated = true;
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::UpdateAuthFactor(
    const ::user_data_auth::UpdateAuthFactorRequest& request,
    UpdateAuthFactorCallback callback) {
  ::user_data_auth::UpdateAuthFactorReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::RemoveAuthFactor(
    const ::user_data_auth::RemoveAuthFactorRequest& request,
    RemoveAuthFactorCallback callback) {
  ::user_data_auth::RemoveAuthFactorReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (TestApi::Get()->service_is_available_ ||
      TestApi::Get()->service_reported_not_available_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  TestApi::Get()->service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeUserDataAuthClient::SetEcryptfsUserHome(
    const cryptohome::AccountIdentifier& cryptohome_id,
    bool use_ecryptfs) {
  if (use_ecryptfs)
    ecryptfs_user_homes_.insert(cryptohome_id);
  else
    ecryptfs_user_homes_.erase(cryptohome_id);
}

void FakeUserDataAuthClient::RunPendingWaitForServiceToBeAvailableCallbacks() {
  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(false);
}

template <typename ReplyType>
void FakeUserDataAuthClient::ReturnProtobufMethodCallback(
    const ReplyType& reply,
    DBusMethodCallback<ReplyType> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

void FakeUserDataAuthClient::OnDircryptoMigrationProgressUpdated() {
  dircrypto_migration_progress_++;

  if (dircrypto_migration_progress_ >= kDircryptoMigrationMaxProgress) {
    NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
        dircrypto_migration_progress_, kDircryptoMigrationMaxProgress);
    SetEcryptfsUserHome(last_migrate_to_dircrypto_request_.account_id(), false);
    dircrypto_migration_progress_timer_.Stop();
    return;
  }
  NotifyDircryptoMigrationProgress(::user_data_auth::DircryptoMigrationStatus::
                                       DIRCRYPTO_MIGRATION_IN_PROGRESS,
                                   dircrypto_migration_progress_,
                                   kDircryptoMigrationMaxProgress);
}

void FakeUserDataAuthClient::NotifyLowDiskSpace(uint64_t disk_free_bytes) {
  ::user_data_auth::LowDiskSpace status;
  status.set_disk_free_bytes(disk_free_bytes);
  for (auto& observer : observer_list_)
    observer.LowDiskSpace(status);
}

void FakeUserDataAuthClient::NotifyDircryptoMigrationProgress(
    ::user_data_auth::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  ::user_data_auth::DircryptoMigrationProgress progress;
  progress.set_status(status);
  progress.set_current_bytes(current);
  progress.set_total_bytes(total);
  for (auto& observer : observer_list_)
    observer.DircryptoMigrationProgress(progress);
}

bool FakeUserDataAuthClient::IsEcryptfsUserHome(
    const cryptohome::AccountIdentifier& cryptohome_id) {
  return base::Contains(ecryptfs_user_homes_, cryptohome_id);
}

void FakeUserDataAuthClient::CreateUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) {
  base::CreateDirectory(GetUserProfileDir(account_id));
}

base::FilePath FakeUserDataAuthClient::GetUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) const {
  DCHECK(!user_data_dir_.empty());
  // "u-" below corresponds to chrome::kProfileDirPrefix,
  // which can not be easily included.
  std::string user_dir =
      "u-" + UserDataAuthClient::GetStubSanitizedUsername(account_id);
  base::FilePath profile_dir = user_data_dir_.Append(user_dir);
  return profile_dir;
}

bool FakeUserDataAuthClient::UserExists(
    const cryptohome::AccountIdentifier& account_id) const {
  if (users_.find(account_id) != std::end(users_)) {
    return true;
  }

  base::ScopedAllowBlockingForTesting allow_io;
  bool result = base::PathExists(GetUserProfileDir(account_id));
  LOG(INFO) << "User " << (result ? "exists" : "does not exist")
            << " profile dir";
  return result;
}

const FakeUserDataAuthClient::AuthSessionData*
FakeUserDataAuthClient::GetAuthenticatedAuthSession(
    const std::string& auth_session_id,
    ::user_data_auth::CryptohomeErrorCode* error) const {
  auto auth_session = auth_sessions_.find(auth_session_id);

  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    *error = ::user_data_auth::CryptohomeErrorCode::
        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN;
  }

  // Check if the AuthSession is properly authenticated.
  if (!auth_session->second.authenticated) {
    LOG(ERROR) << "AuthSession is not authenticated";
    *error = ::user_data_auth::CryptohomeErrorCode::
        CRYPTOHOME_ERROR_INVALID_ARGUMENT;
  }

  return &auth_session->second;
}

void FakeUserDataAuthClient::AddExistingUser(
    const cryptohome::AccountIdentifier& account_id) {
  // Insert user without any associated keys.
  const auto [_, was_inserted] =
      users_.insert({account_id, UserCryptohomeState()});
  DCHECK(was_inserted) << "User already exists: " << account_id.account_id();
}

}  // namespace ash
