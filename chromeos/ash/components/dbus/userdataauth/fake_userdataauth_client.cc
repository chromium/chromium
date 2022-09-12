// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"

#include <utility>

#include "base/check.h"
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
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
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

// Strings concatenated with the account id to obtain a user's profile
// directory name. The prefix "u-" below corresponds to
// `chrome::kProfileDirPrefix` (which can not be easily included here) and
// "-hash" is as in `GetStubSanitizedUsername`.
const std::string kUserDataDirNamePrefix = "u-";
const std::string kUserDataDirNameSuffix = "-hash";

}  // namespace

struct FakeUserDataAuthClient::UserCryptohomeState {
  // Maps labels to auth factors.
  base::flat_map<std::string, FakeAuthFactor> auth_factors;

  // A flag describing how we pretend that the user's home directory is
  // encrypted.
  HomeEncryptionMethod home_encryption_method =
      HomeEncryptionMethod::kDirCrypto;
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

// `OverloadedFunctor` and `FunctorWithReturnType` are used to implement
// `Overload`, which constructs a visitor appropriate for use with
// `absl::visit` from lambdas for each case.

// A functor combining the `operator()` definitions of a list of functors into
// a single functor with overloaded `operator()`.
template <class... Functors>
struct OverloadedFunctor : Functors... {
  using Functors::operator()...;
};

// Used to fix the return type of a functor with overloaded `operator()`.
// This is useful in case the `operator()` overloads have different return
// types, but all return types are convertible into the intended fixed
// `ReturnType`.
template <class ReturnType, class Functor>
struct FunctorWithReturnType {
  template <class Arg>
  ReturnType operator()(Arg&& arg) {
    return functor(std::forward<Arg>(arg));
  }

  Functor functor;
};

// `Overload` constructs a visitor appropriate for use with `absl::visit` from
// a number of lambdas for each case. The return type of each provided lambda
// must be convertible to `ReturnType`, and the `operator()` of the combined
// visitor will always return `ReturnType`.
template <class ReturnType, class... Functors>
FunctorWithReturnType<ReturnType, OverloadedFunctor<Functors...>> Overload(
    Functors... functors) {
  return {{std::move(functors)...}};
}

absl::optional<cryptohome::KeyData> FakeAuthFactorToKeyData(
    std::string label,
    const FakeAuthFactor& factor) {
  return absl::visit(
      Overload<absl::optional<cryptohome::KeyData>>(
          [&](const PasswordFactor& password) {
            cryptohome::KeyData data;
            data.set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
            data.set_label(std::move(label));
            return data;
          },
          [&](const PinFactor& pin) {
            cryptohome::KeyData data;
            data.set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
            data.set_label(std::move(label));
            data.mutable_policy()->set_low_entropy_credential(true);
            data.mutable_policy()->set_auth_locked(pin.locked);
            return data;
          },
          [&](const RecoveryFactor&) { return absl::nullopt; },
          [&](const KioskFactor& kiosk) {
            cryptohome::KeyData data;
            data.set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
            data.set_label(std::move(label));
            return data;
          }),
      factor);
}

absl::optional<user_data_auth::AuthFactor> FakeAuthFactorToAuthFactor(
    std::string label,
    const FakeAuthFactor& factor) {
  return absl::visit(
      Overload<absl::optional<user_data_auth::AuthFactor>>(
          [&](const PasswordFactor& password) {
            user_data_auth::AuthFactor result;
            result.set_label(std::move(label));
            result.set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
            result.mutable_password_metadata();
            return result;
          },
          [&](const PinFactor& pin) {
            user_data_auth::AuthFactor result;
            result.set_label(std::move(label));
            result.set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);
            result.mutable_pin_metadata()->set_auth_locked(pin.locked);
            return result;
          },
          [&](const RecoveryFactor&) {
            user_data_auth::AuthFactor result;
            result.set_label(std::move(label));
            result.set_type(
                user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY);
            result.mutable_cryptohome_recovery_metadata();
            return result;
          },
          [&](const KioskFactor& kiosk) {
            user_data_auth::AuthFactor result;
            result.set_label(std::move(label));
            result.set_type(user_data_auth::AUTH_FACTOR_TYPE_KIOSK);
            result.mutable_kiosk_metadata();
            return result;
          }),
      factor);
}

// Turns a cryptohome::Key into a pair of label and FakeAuthFactor.
std::pair<std::string, FakeAuthFactor> KeyToFakeAuthFactor(
    const cryptohome::Key& key,
    bool save_secret) {
  const cryptohome::KeyData& data = key.data();
  const std::string& label = data.label();
  CHECK_NE(label, "") << "Key label must not be empty string";
  absl::optional<std::string> secret = absl::nullopt;
  if (save_secret && key.has_secret()) {
    secret = key.secret();
  }

  switch (data.type()) {
    case cryptohome::KeyData::KEY_TYPE_CHALLENGE_RESPONSE:
    case cryptohome::KeyData::KEY_TYPE_FINGERPRINT:
      LOG(FATAL) << "Unsupported key type: " << data.type();
      __builtin_unreachable();
    case cryptohome::KeyData::KEY_TYPE_PASSWORD:
      if (data.has_policy() && data.policy().low_entropy_credential()) {
        return {label, PinFactor{.pin = secret, .locked = false}};
      }
      return {label, PasswordFactor{.password = secret}};
    case cryptohome::KeyData::KEY_TYPE_KIOSK:
      return {label, KioskFactor{}};
  }
}

// Turns AuthFactor+AuthInput into a pair of label and FakeAuthFactor.
std::pair<std::string, FakeAuthFactor> AuthFactorWithInputToFakeAuthFactor(
    const user_data_auth::AuthFactor& factor,
    const user_data_auth::AuthInput& input,
    bool save_secret) {
  const std::string& label = factor.label();
  CHECK_NE(label, "") << "Key label must not be empty string";

  absl::optional<std::string> secret = absl::nullopt;
  if (save_secret) {
    if (factor.type() == user_data_auth::AUTH_FACTOR_TYPE_PASSWORD) {
      secret = input.password_input().secret();
    } else if (factor.type() == user_data_auth::AUTH_FACTOR_TYPE_PIN) {
      secret = input.pin_input().secret();
    }
  }

  switch (factor.type()) {
    case user_data_auth::AUTH_FACTOR_TYPE_UNSPECIFIED:
      LOG(FATAL) << "Chrome should never send Unspecified auth factor.";
      __builtin_unreachable();
    case user_data_auth::AUTH_FACTOR_TYPE_PIN:
      return {label, PinFactor{.pin = secret, .locked = false}};
    case user_data_auth::AUTH_FACTOR_TYPE_PASSWORD:
      return {label, PasswordFactor{.password = secret}};
    case user_data_auth::AUTH_FACTOR_TYPE_KIOSK:
      return {label, KioskFactor{}};
    case user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY:
      return {label, RecoveryFactor{}};
    default:
      NOTREACHED();
      __builtin_unreachable();
  }
}

bool CheckCredentialsViaAuthFactor(const FakeAuthFactor& factor,
                                   const std::string& secret) {
  return absl::visit(
      Overload<bool>(
          [&](const PasswordFactor& password) {
            return password.password == secret;
          },
          [&](const PinFactor& pin) { return pin.pin == secret; },
          [&](const RecoveryFactor& recovery) {
            LOG(FATAL) << "Checking recovery key is not allowed";
            return false;
          },
          [&](const KioskFactor& kiosk) {
            // Kiosk key secrets are derived from app ids and don't leave
            // cryptohome, so there's nothing to check.
            return true;
          }),
      factor);
}

// Helper that automatically sends a reply struct to a supplied callback when
// it goes out of scope. Basically a specialized `absl::Cleanup` or
// `std::scope_exit`.
template <typename ReplyType>
class ReplyOnReturn {
 public:
  explicit ReplyOnReturn(ReplyType* reply,
                         chromeos::DBusMethodCallback<ReplyType> callback)
      : reply_(reply), callback_(std::move(callback)) {}
  ReplyOnReturn(const ReplyOnReturn<ReplyType>&) = delete;

  ~ReplyOnReturn() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), *reply_));
  }

  ReplyOnReturn<ReplyType>& operator=(const ReplyOnReturn<ReplyType>&) = delete;

 private:
  raw_ptr<ReplyType> reply_;
  chromeos::DBusMethodCallback<ReplyType> callback_;
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
  if (instance_) {
    return instance_;
  }

  // TestApi assumes that the FakeUserDataAuthClient singleton is initialized.
  if (FakeUserDataAuthClient::Get() == nullptr) {
    return nullptr;
  }

  instance_ = new TestApi(FakeUserDataAuthClient::Get());
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

void FakeUserDataAuthClient::TestApi::SetHomeEncryptionMethod(
    const cryptohome::AccountIdentifier& cryptohome_id,
    HomeEncryptionMethod method) {
  auto user_it = client_->users_.find(cryptohome_id);
  if (user_it == std::end(client_->users_)) {
    LOG(ERROR) << "User does not exist: " << cryptohome_id.account_id();
    // TODO(crbug.com/1334538): Some existing tests rely on us creating the
    // user here, but new tests shouldn't. Eventually this should crash.
    user_it =
        client_->users_.insert({cryptohome_id, UserCryptohomeState()}).first;
  }
  DCHECK(user_it != std::end(client_->users_));
  UserCryptohomeState& user_state = user_it->second;
  user_state.home_encryption_method = method;
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

void FakeUserDataAuthClient::TestApi::AddExistingUser(
    const cryptohome::AccountIdentifier& account_id) {
  const auto [user_it, was_inserted] =
      client_->users_.insert({std::move(account_id), UserCryptohomeState()});
  if (!was_inserted) {
    LOG(WARNING) << "User already exists: " << user_it->first.account_id();
    return;
  }

  const absl::optional<base::FilePath> profile_dir =
      client_->GetUserProfileDir(user_it->first);
  if (!profile_dir) {
    LOG(WARNING) << "User data directory has not been set, will not create "
                    "user profile directory";
    return;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::CreateDirectory(*profile_dir));
}

absl::optional<base::FilePath>
FakeUserDataAuthClient::TestApi::GetUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) const {
  return client_->GetUserProfileDir(account_id);
}

void FakeUserDataAuthClient::TestApi::AddKey(
    const cryptohome::AccountIdentifier& account_id,
    const cryptohome::Key& key) {
  const auto user_it = client_->users_.find(account_id);
  CHECK(user_it != std::end(client_->users_)) << "User doesn't exist";
  UserCryptohomeState& user_state = user_it->second;
  const auto [factor_it, was_inserted] = user_state.auth_factors.insert(
      KeyToFakeAuthFactor(key, TestApi::Get()->enable_auth_check_));
  CHECK(was_inserted) << "Factor already exists";
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
  last_mount_request_ = request;
  ++mount_request_count_;

  ::user_data_auth::MountReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (cryptohome_error_ !=
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(cryptohome_error_);
    return;
  }

  if (request.guest_mount()) {
    cryptohome::AccountIdentifier account_id;
    account_id.set_account_id(kGuestUserName);
    reply.set_sanitized_username(GetStubSanitizedUsername(account_id));
    return;
  }

  // TODO(crbug.com/1334538): We should get rid of mount_create_required_
  // and instead check whether the user exists or not here. Tests would then
  // need to set up a user (or not).
  if (TestApi::Get()->mount_create_required_ && !request.has_create()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }

  const cryptohome::AccountIdentifier* account_id;
  if (request.has_account()) {
    account_id = &request.account();
  } else {
    auto auth_session = auth_sessions_.find(request.auth_session_id());
    CHECK(auth_session != std::end(auth_sessions_))
        << "Invalid account session";
    account_id = &auth_session->second.account;
  }
  DCHECK(account_id);

  const auto [user_it, was_inserted] =
      users_.insert({*account_id, UserCryptohomeState()});
  UserCryptohomeState& user_state = user_it->second;

  if (!was_inserted) {
    const cryptohome::Key& key = request.authorization().key();
    switch (AuthenticateViaAuthFactors(*account_id,
                                       /*factor_label=*/key.data().label(),
                                       /*secret=*/key.secret(),
                                       /*wildcard_allowed=*/true)) {
      case AuthResult::kAuthSuccess:
        break;
      case AuthResult::kUserNotFound:
        NOTREACHED();
        break;
      case AuthResult::kFactorNotFound:
        reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
        return;
      case AuthResult::kAuthFailed:
        reply.set_error(
            ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
        return;
    }
  }

  // The real cryptohome supports this, but it's not used in chrome at the
  // moment and thus not properly supported by fake cryptohome.
  LOG_IF(WARNING, !was_inserted && request.has_create())
      << "UserDataAuth::Mount called with create field for existing user: "
      << account_id->account_id();
  // TODO(crbug.com/1334538): Some tests rely on this working, but those should
  // be migrated.
  LOG_IF(ERROR, was_inserted && !request.has_create())
      << "UserDataAuth::Mount called without create field for nonexistant "
         "user: "
      << account_id->account_id();

  if (request.has_create()) {
    const user_data_auth::CreateRequest& create_req = request.create();
    CHECK_EQ(1, create_req.keys().size())
        << "UserDataAuth::Mount called with `create` that does not contain "
           "precisely one key";
    user_state.auth_factors.insert(KeyToFakeAuthFactor(
        create_req.keys()[0], TestApi::Get()->enable_auth_check_));
  }

  const bool is_ecryptfs =
      user_state.home_encryption_method == HomeEncryptionMethod::kEcryptfs;
  if (is_ecryptfs && !request.to_migrate_from_ecryptfs() &&
      request.force_dircrypto_if_available()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION);
    return;
  }

  reply.set_sanitized_username(GetStubSanitizedUsername(*account_id));
}

void FakeUserDataAuthClient::Remove(
    const ::user_data_auth::RemoveRequest& request,
    RemoveCallback callback) {
  ::user_data_auth::RemoveReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  cryptohome::AccountIdentifier account_id;
  if (request.has_identifier()) {
    account_id = request.identifier();
  } else {
    auto auth_session = auth_sessions_.find(request.auth_session_id());
    CHECK(auth_session != std::end(auth_sessions_)) << "Invalid auth session";
    account_id = auth_session->second.account;
  }

  const auto user_it = users_.find(account_id);
  if (user_it == users_.end()) {
    reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }

  const absl::optional<base::FilePath> profile_dir =
      GetUserProfileDir(account_id);
  if (profile_dir) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    CHECK(base::DeletePathRecursively(*profile_dir));
  } else {
    LOG(WARNING) << "User data directory has not been set, will not delete "
                    "user profile directory";
  }

  users_.erase(user_it);
  if (!request.auth_session_id().empty()) {
    // Removing the user also invalidates the AuthSession.
    auth_sessions_.erase(request.auth_session_id());
  }
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

    absl::optional<cryptohome::KeyData> key_data =
        FakeAuthFactorToKeyData(label, factor);
    if (key_data.has_value()) {
      reply.mutable_key_data()->Add(std::move(*key_data));
    } else {
      LOG(WARNING) << "Ignoring auth factor incompatible with legacy API: "
                   << label;
    }
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

  last_unlock_webauthn_secret_ = request.unlock_webauthn_secret();

  const cryptohome::Key& key = request.authorization_request().key();
  switch (AuthenticateViaAuthFactors(
      request.account_id(), /*factor_label=*/key.data().label(),
      /*secret=*/key.secret(), /*wildcard_allowed=*/true)) {
    case AuthResult::kAuthSuccess:
      // Empty reply denotes a successful check.
      break;
    case AuthResult::kUserNotFound:
      reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
      break;
    case AuthResult::kFactorNotFound:
      reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
      break;
    case AuthResult::kAuthFailed:
      reply.set_error(
          ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
      break;
  }
}

void FakeUserDataAuthClient::AddKey(
    const ::user_data_auth::AddKeyRequest& request,
    AddKeyCallback callback) {
  ::user_data_auth::AddKeyReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const cryptohome::AccountIdentifier& account_id = request.account_id();
  const bool clobber_if_exists = request.clobber_if_exists();
  const cryptohome::Key& new_key = request.key();

  auto user_it = users_.find(account_id);
  if (user_it == std::end(users_)) {
    // TODO(crbug.com/1334538): Cryptohome would not create a new user here,
    // but many tests rely on it. New tests shouldn't rely on this behavior.
    LOG(ERROR) << "Need to create new user: " << account_id.account_id();
    user_it = users_.insert(user_it, {account_id, UserCryptohomeState()});
  }
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  auto [new_label, new_factor] =
      KeyToFakeAuthFactor(new_key, TestApi::Get()->enable_auth_check_);
  CHECK(clobber_if_exists || !user_state.auth_factors.contains(new_label))
      << "Key exists, will not clobber: " << new_label;
  user_state.auth_factors[std::move(new_label)] = std::move(new_factor);
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
  ::user_data_auth::MigrateKeyReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const cryptohome::Key& key = request.authorization_request().key();
  std::string matched_factor_label;
  switch (AuthenticateViaAuthFactors(
      request.account_id(), /*factor_label=*/key.data().label(),
      /*secret=*/key.secret(), /*wildcard_allowed=*/true,
      &matched_factor_label)) {
    case AuthResult::kAuthSuccess:
      // Can proceed to the migration.
      break;
    case AuthResult::kUserNotFound:
      reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
      return;
    case AuthResult::kFactorNotFound:
      reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
      return;
    case AuthResult::kAuthFailed:
      reply.set_error(
          ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
      return;
  }
  UserCryptohomeState& user_state = users_[request.account_id()];

  // Update the fake auth factor according to the new secret.
  cryptohome::Key new_key = key;
  if (new_key.data().label().empty())
    new_key.mutable_data()->set_label(matched_factor_label);
  new_key.set_secret(request.secret());
  const auto [new_label, new_factor] =
      KeyToFakeAuthFactor(new_key, TestApi::Get()->enable_auth_check_);
  DCHECK_EQ(new_label, matched_factor_label);
  user_state.auth_factors[matched_factor_label] = new_factor;
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
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const cryptohome::AccountIdentifier& account_id = request.account_id();

  const auto user_it = users_.find(account_id);
  if (user_it == std::end(users_)) {
    // TODO(crbug.com/1334538): New tests shouldn't rely on this behavior and
    // instead set up the user first.
    LOG(ERROR) << "User does not exist: " << account_id.account_id();
    reply.set_needs_dircrypto_migration(false);
    return;
  }
  DCHECK(user_it != users_.end());
  const UserCryptohomeState& user_state = user_it->second;

  const bool is_ecryptfs =
      user_state.home_encryption_method == HomeEncryptionMethod::kEcryptfs;
  reply.set_needs_dircrypto_migration(is_ecryptfs);
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
  ::user_data_auth::StartAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  std::string auth_session_id =
      base::StringPrintf(kAuthSessionIdTemplate, next_auth_session_id_++);

  DCHECK_EQ(request.intent(), user_data_auth::AUTH_INTENT_DECRYPT);
  DCHECK_EQ(auth_sessions_.count(auth_session_id), 0u);
  AuthSessionData& session = auth_sessions_[auth_session_id];
  session.id = auth_session_id;
  session.ephemeral =
      (request.flags() & ::user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER) !=
      0;
  session.account = request.account_id();

  if (cryptohome_error_ !=
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(cryptohome_error_);
    return;
  }

  reply.set_auth_session_id(auth_session_id);

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  reply.set_user_exists(user_exists);

  if (user_exists) {
    const UserCryptohomeState& user_state = user_it->second;
    for (const auto& [label, factor] : user_state.auth_factors) {
      absl::optional<cryptohome::KeyData> key_data =
          FakeAuthFactorToKeyData(label, factor);
      if (key_data) {
        reply.mutable_key_label_data()->insert({label, std::move(*key_data)});
      } else {
        LOG(WARNING) << "Ignoring auth factor incompatible with legacy API: "
                     << label;
      }
      absl::optional<user_data_auth::AuthFactor> auth_factor =
          FakeAuthFactorToAuthFactor(label, factor);
      if (key_data) {
        *reply.add_auth_factors() = *auth_factor;
      } else {
        LOG(WARNING)
            << "Ignoring auth factor incompatible with AuthFactor API: "
            << label;
      }
    }
  }

  const std::string& account_id = request.account_id().account_id();
  // See device_local_account.h
  const bool is_kiosk =
      base::EndsWith(account_id, "kiosk-apps.device-local.localhost");
  // TODO(crbug.com/1334538): Some tests expect that kiosk or gaia keys exist
  // for existing users, but don't set those keys up. Until those tests are
  // fixed, we explicitly add keys here.
  if (user_exists) {
    if (is_kiosk) {
      // See kCryptohomePublicMountLabel.
      std::string kiosk_label = "publicmount";
      cryptohome::KeyData kiosk_key;
      kiosk_key.set_label(kiosk_label);
      kiosk_key.set_type(cryptohome::KeyData::KEY_TYPE_KIOSK);
      const auto [_, was_inserted] = reply.mutable_key_label_data()->insert(
          {std::move(kiosk_label), std::move(kiosk_key)});
      LOG_IF(ERROR, was_inserted)
          << "Listing kiosk key even though it was not set up";
    } else {
      // See kCryptohomeGaiaKeyLabel.
      std::string gaia_label = "gaia";
      cryptohome::KeyData gaia_key;
      gaia_key.set_label(gaia_label);
      gaia_key.set_type(cryptohome::KeyData::KEY_TYPE_PASSWORD);
      const auto [_, was_inserted] = reply.mutable_key_label_data()->insert(
          {std::move(gaia_label), std::move(gaia_key)});
      LOG_IF(ERROR, was_inserted)
          << "Listing gaia key even though it was not set up";
    }
  }
}

void FakeUserDataAuthClient::ListAuthFactors(
    const ::user_data_auth::ListAuthFactorsRequest& request,
    ListAuthFactorsCallback callback) {
  ::user_data_auth::ListAuthFactorsReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (cryptohome_error_ !=
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(cryptohome_error_);
    return;
  }

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  if (!user_exists) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }

  const UserCryptohomeState& user_state = user_it->second;
  for (const auto& [label, factor] : user_state.auth_factors) {
    absl::optional<user_data_auth::AuthFactor> auth_factor =
        FakeAuthFactorToAuthFactor(label, factor);
    if (auth_factor) {
      *reply.add_configured_auth_factors() = *auth_factor;
    } else {
      LOG(WARNING) << "Ignoring auth factor incompatible with AuthFactor API: "
                   << label;
    }
  }

  const std::string& account_id = request.account_id().account_id();
  // See device_local_account.h
  const bool is_kiosk =
      base::EndsWith(account_id, "kiosk-apps.device-local.localhost");

  if (is_kiosk) {
    reply.add_supported_auth_factors(user_data_auth::AUTH_FACTOR_TYPE_KIOSK);
  } else {
    reply.add_supported_auth_factors(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
    reply.add_supported_auth_factors(user_data_auth::AUTH_FACTOR_TYPE_PIN);
    reply.add_supported_auth_factors(
        user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY);
  }
}

void FakeUserDataAuthClient::AuthenticateAuthSession(
    const ::user_data_auth::AuthenticateAuthSessionRequest& request,
    AuthenticateAuthSessionCallback callback) {
  last_authenticate_auth_session_request_ = request;
  ::user_data_auth::AuthenticateAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const std::string auth_session_id = request.auth_session_id();

  const auto it = auth_sessions_.find(auth_session_id);
  if (it == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  AuthSessionData& auth_session = it->second;

  if (cryptohome_error_ !=
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(cryptohome_error_);
    return;
  }

  const cryptohome::Key& key = request.authorization().key();
  switch (AuthenticateViaAuthFactors(auth_session.account,
                                     /*factor_label=*/key.data().label(),
                                     /*secret=*/key.secret(),
                                     /*wildcard_allowed=*/false)) {
    case AuthResult::kAuthSuccess:
      // Proceed to marking the auth session authenticated.
      break;
    case AuthResult::kUserNotFound:
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
      return;
    case AuthResult::kFactorNotFound:
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_KEY_NOT_FOUND);
      return;
    case AuthResult::kAuthFailed:
      reply.set_error(::user_data_auth::CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
      return;
  }

  auth_session.authenticated = true;
  reply.set_authenticated(true);
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
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  AuthSessionData& auth_session = session_it->second;
  if (!auth_session.ephemeral) {
    LOG(ERROR) << "Non-ephemeral AuthSession used with PrepareEphemeralVault";
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }
  cryptohome::AccountIdentifier account = auth_session.account;
  // Ephemeral mount does not require session to be authenticated;
  // It authenticates session instead.
  if (auth_session.authenticated) {
    LOG(ERROR) << "AuthSession is authenticated";
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }
  auth_session.authenticated = true;

  reply.set_sanitized_username(GetStubSanitizedUsername(account));
}

void FakeUserDataAuthClient::CreatePersistentUser(
    const ::user_data_auth::CreatePersistentUserRequest& request,
    CreatePersistentUserCallback callback) {
  ::user_data_auth::CreatePersistentUserReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  AuthSessionData& auth_session = session_it->second;

  if (auth_session.ephemeral) {
    LOG(ERROR) << "Ephemeral AuthSession used with CreatePersistentUser";
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }

  const auto [_, was_inserted] =
      users_.insert({auth_session.account, UserCryptohomeState()});

  if (!was_inserted) {
    LOG(ERROR) << "User already exists: " << auth_session.account.account_id();
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
    return;
  }

  auth_session.authenticated = true;
}

void FakeUserDataAuthClient::PreparePersistentVault(
    const ::user_data_auth::PreparePersistentVaultRequest& request,
    PreparePersistentVaultCallback callback) {
  ::user_data_auth::PreparePersistentVaultReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    reply.set_error(error);
    return;
  }

  if (authenticated_auth_session->ephemeral) {
    LOG(ERROR) << "Ephemeral AuthSession used with PreparePersistentVault";
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }

  if (!users_.contains(authenticated_auth_session->account)) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }

  reply.set_sanitized_username(
      GetStubSanitizedUsername(authenticated_auth_session->account));
}

void FakeUserDataAuthClient::PrepareVaultForMigration(
    const ::user_data_auth::PrepareVaultForMigrationRequest& request,
    PrepareVaultForMigrationCallback callback) {
  ::user_data_auth::PrepareVaultForMigrationReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    reply.set_error(error);
    return;
  }

  if (!users_.contains(authenticated_auth_session->account)) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }
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
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);
  if (session == nullptr) {
    ReturnProtobufMethodCallback(reply, std::move(callback));
    return;
  }

  auto user_it = users_.find(session->account);
  CHECK(user_it != std::end(users_))
      << "User associated with session does not exist";
  UserCryptohomeState& user_state = user_it->second;

  auto [new_label, new_factor] = AuthFactorWithInputToFakeAuthFactor(
      request.auth_factor(), request.auth_input(),
      TestApi::Get()->enable_auth_check_);
  CHECK(!user_state.auth_factors.contains(new_label))
      << "Key exists, will not clobber: " << new_label;
  user_state.auth_factors[std::move(new_label)] = std::move(new_factor);

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
    // TODO(b/241259026): Check if key is present and if secret matches.
    auth_session->second.authenticated = true;
  }
  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::UpdateAuthFactor(
    const ::user_data_auth::UpdateAuthFactorRequest& request,
    UpdateAuthFactorCallback callback) {
  ::user_data_auth::UpdateAuthFactorReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);
  if (session == nullptr) {
    ReturnProtobufMethodCallback(reply, std::move(callback));
    return;
  }

  auto user_it = users_.find(session->account);
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  // Update the fake auth factor according to the new secret.
  auto [new_label, new_factor] = AuthFactorWithInputToFakeAuthFactor(
      request.auth_factor(), request.auth_input(),
      TestApi::Get()->enable_auth_check_);
  CHECK_EQ(new_label, request.auth_factor_label());
  CHECK(user_state.auth_factors.contains(new_label))
      << "Key does not exist: " << new_label;
  user_state.auth_factors[std::move(new_label)] = std::move(new_factor);

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::RemoveAuthFactor(
    const ::user_data_auth::RemoveAuthFactorRequest& request,
    RemoveAuthFactorCallback callback) {
  ::user_data_auth::RemoveAuthFactorReply reply;

  auto error = ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);
  if (session == nullptr) {
    ReturnProtobufMethodCallback(reply, std::move(callback));
    return;
  }
  auto user_it = users_.find(session->account);
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  const std::string& label = request.auth_factor_label();
  DCHECK(!label.empty());
  bool erased = user_state.auth_factors.erase(label) > 0;

  if (!erased)
    reply.set_error(
        ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND);

  ReturnProtobufMethodCallback(reply, std::move(callback));
}

void FakeUserDataAuthClient::GetRecoveryRequest(
    const ::user_data_auth::GetRecoveryRequestRequest& request,
    GetRecoveryRequestCallback callback) {
  ::user_data_auth::GetRecoveryRequestReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
}

void FakeUserDataAuthClient::GetAuthSessionStatus(
    const ::user_data_auth::GetAuthSessionStatusRequest& request,
    GetAuthSessionStatusCallback callback) {
  ::user_data_auth::GetAuthSessionStatusReply reply;

  const std::string auth_session_id = request.auth_session_id();
  auto auth_session = auth_sessions_.find(auth_session_id);

  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    reply.set_error(::user_data_auth::CryptohomeErrorCode::
                        CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
  } else if (auth_session->second.authenticated) {
    reply.set_status(::user_data_auth::AUTH_SESSION_STATUS_AUTHENTICATED);
    // Use 5 minutes timeout - as if auth session has just started.
    reply.set_time_left(5 * 60);
  } else {
    reply.set_status(
        ::user_data_auth::AUTH_SESSION_STATUS_FURTHER_FACTOR_REQUIRED);
  }

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

void FakeUserDataAuthClient::RunPendingWaitForServiceToBeAvailableCallbacks() {
  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run(false);
}

FakeUserDataAuthClient::AuthResult
FakeUserDataAuthClient::AuthenticateViaAuthFactors(
    const cryptohome::AccountIdentifier& account_id,
    const std::string& factor_label,
    const std::string& secret,
    bool wildcard_allowed,
    std::string* matched_factor_label) const {
  if (!TestApi::Get()->enable_auth_check_)
    return AuthResult::kAuthSuccess;

  const auto user_it = users_.find(account_id);
  if (user_it == std::end(users_))
    return AuthResult::kUserNotFound;
  const UserCryptohomeState& user_state = user_it->second;

  if (wildcard_allowed && factor_label.empty()) {
    // Do a wildcard match (it's only used for legacy APIs): try the secret
    // against every credential.
    for (const auto& [candidate_label, candidate_factor] :
         user_state.auth_factors) {
      if (CheckCredentialsViaAuthFactor(candidate_factor, secret)) {
        if (matched_factor_label)
          *matched_factor_label = candidate_label;
        return AuthResult::kAuthSuccess;
      }
    }
    // It's not well-defined which error is returned on a failed wildcard
    // authentication, but we follow what the real cryptohome does (at least in
    // CheckKey).
    return AuthResult::kAuthFailed;
  }

  const auto factor_it = user_state.auth_factors.find(factor_label);
  if (factor_it == std::end(user_state.auth_factors))
    return AuthResult::kFactorNotFound;
  const auto& [label, factor] = *factor_it;
  if (!CheckCredentialsViaAuthFactor(factor, secret))
    return AuthResult::kAuthFailed;
  if (matched_factor_label)
    *matched_factor_label = label;
  return AuthResult::kAuthSuccess;
}

template <typename ReplyType>
void FakeUserDataAuthClient::ReturnProtobufMethodCallback(
    const ReplyType& reply,
    chromeos::DBusMethodCallback<ReplyType> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), reply));
}

void FakeUserDataAuthClient::OnDircryptoMigrationProgressUpdated() {
  dircrypto_migration_progress_++;

  if (dircrypto_migration_progress_ >= kDircryptoMigrationMaxProgress) {
    NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
        dircrypto_migration_progress_, kDircryptoMigrationMaxProgress);
    const auto user_it =
        users_.find(last_migrate_to_dircrypto_request_.account_id());
    DCHECK(user_it != std::end(users_))
        << "User for dircrypto migration does not exist";

    UserCryptohomeState& user_state = user_it->second;
    user_state.home_encryption_method = HomeEncryptionMethod::kDirCrypto;
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

absl::optional<base::FilePath> FakeUserDataAuthClient::GetUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) const {
  if (!user_data_dir_.has_value())
    return absl::nullopt;

  std::string user_dir_base_name =
      kUserDataDirNamePrefix + account_id.account_id() + kUserDataDirNameSuffix;
  const base::FilePath profile_dir =
      user_data_dir_->Append(std::move(user_dir_base_name));
  return profile_dir;
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
    return nullptr;
  }

  // Check if the AuthSession is properly authenticated.
  if (!auth_session->second.authenticated) {
    LOG(ERROR) << "AuthSession is not authenticated";
    *error = ::user_data_auth::CryptohomeErrorCode::
        CRYPTOHOME_ERROR_INVALID_ARGUMENT;
    return nullptr;
  }

  return &auth_session->second;
}

void FakeUserDataAuthClient::SetUserDataDir(base::FilePath path) {
  CHECK(!user_data_dir_.has_value());
  user_data_dir_ = std::move(path);

  std::string pattern = kUserDataDirNamePrefix + "*" + kUserDataDirNameSuffix;
  base::FileEnumerator e(*user_data_dir_, /*recursive=*/false,
                         base::FileEnumerator::DIRECTORIES, std::move(pattern));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    const base::FilePath base_name = name.BaseName();
    DCHECK(base::StartsWith(base_name.value(), kUserDataDirNamePrefix));
    DCHECK(base::EndsWith(base_name.value(), kUserDataDirNameSuffix));

    // Remove kUserDataDirNamePrefix from front and kUserDataDirNameSuffix from
    // end to obtain account id.
    std::string account_id_str(
        base_name.value().begin() + kUserDataDirNamePrefix.size(),
        base_name.value().end() - kUserDataDirNameSuffix.size());

    cryptohome::AccountIdentifier account_id;
    account_id.set_account_id(std::move(account_id_str));

    // This does intentionally not override existing entries.
    users_.insert({std::move(account_id), UserCryptohomeState()});
  }
}

}  // namespace ash
