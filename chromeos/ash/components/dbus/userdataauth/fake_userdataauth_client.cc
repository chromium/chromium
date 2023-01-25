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
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash {

using ::user_data_auth::CryptohomeErrorCode;

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
struct SmartCardFactor {
  std::string public_key_spki_der;
};

struct KioskFactor {};

using FakeAuthFactor = absl::variant<PasswordFactor,
                                     PinFactor,
                                     RecoveryFactor,
                                     KioskFactor,
                                     SmartCardFactor>;

// Strings concatenated with the account id to obtain a user's profile
// directory name. The prefix "u-" below corresponds to
// `chrome::kProfileDirPrefix` (which can not be easily included here) and
// "-hash" is as in `GetStubSanitizedUsername`.
const std::string kUserDataDirNamePrefix = "u-";
const std::string kUserDataDirNameSuffix = "-hash";

// Label of the recovery auth factor.
const std::string kCryptohomeRecoveryKeyLabel = "recovery";
// Label of the kiosk auth factor.
const std::string kCryptohomePublicMountLabel = "publicmount";
// Label of the GAIA password key
const std::string kCryptohomeGaiaKeyLabel = "gaia";

}  // namespace

struct FakeUserDataAuthClient::UserCryptohomeState {
  // Maps labels to auth factors.
  base::flat_map<std::string, FakeAuthFactor> auth_factors;

  // A flag describing how we pretend that the user's home directory is
  // encrypted.
  HomeEncryptionMethod home_encryption_method =
      HomeEncryptionMethod::kDirCrypto;

  // A flag describing how we pretend that the user's home directory migration
  // was not completed correctly.
  bool incomplete_migration = false;
};

namespace {

// Interval to update the progress of MigrateToDircrypto in milliseconds.
constexpr int kDircryptoMigrationUpdateIntervalMs = 200;
// The number of updates the MigrateToDircrypto will send before it completes.
constexpr uint64_t kDircryptoMigrationMaxProgress = 15;

// Timeout after which an authenticated session is destroyed by the real
// cryptohome service.
constexpr int kSessionTimeoutSeconds = 5 * 60;

// Template for auth session ID.
constexpr char kAuthSessionIdTemplate[] = "AuthSession-%d";

// Guest username constant that mirrors the one in real cryptohome
constexpr char kGuestUserName[] = "$guest";

// Used to track the global fake instance. This global fake instance is created
// in the first call to FakeUserDataAuth::Get(). During browser startup in
// browser tests and cros-linux, the global instance pointer in
// userdataauth_client.cc is set to the address of this global fake instance.
// During shutdown, the fake instance is deleted in the same way as the normal
// UserDataAuth instance would be deleted. We do this to stay as faithful as
// possible to the real implementation.
// However, browser tests can access and configure the fake instance via the
// TestApi or CryptohomeMixin even before the browser starts, for example in
// the constructor of a browser test fixture.
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
          [&](const SmartCardFactor& smart_card) {
            cryptohome::KeyData data;
            data.set_type(cryptohome::KeyData::KEY_TYPE_CHALLENGE_RESPONSE);
            data.set_label(std::move(label));
            data.add_challenge_response_key()->set_public_key_spki_der(
                smart_card.public_key_spki_der);
            // TODO (b/241259026): populate algorithms.
            return data;
          },
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
          },
          [&](const SmartCardFactor& smart_card) {
            user_data_auth::AuthFactor result;
            result.set_label(std::move(label));
            result.set_type(user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD);
            result.mutable_smart_card_metadata()->set_public_key_spki_der(
                smart_card.public_key_spki_der);
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
    case cryptohome::KeyData::KEY_TYPE_FINGERPRINT:
      LOG(FATAL) << "Unsupported key type: " << data.type();
      __builtin_unreachable();
    case cryptohome::KeyData::KEY_TYPE_CHALLENGE_RESPONSE:
      return {label,
              SmartCardFactor{
                  .public_key_spki_der =
                      data.challenge_response_key(0).public_key_spki_der()}};
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
    case user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD: {
      std::string key = factor.smart_card_metadata().public_key_spki_der();
      return {label, SmartCardFactor{.public_key_spki_der = key}};
    }
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
          },
          [&](const SmartCardFactor& smart_card) {
            LOG(FATAL) << "Checking smart card key is not implemented yet";
            return false;
          }),
      factor);
}

template <class FakeFactorType>
bool ContainsFakeFactor(
    const base::flat_map<std::string, FakeAuthFactor>& factors) {
  const auto it =
      base::ranges::find_if(factors, [](const auto label_factor_pair) {
        const FakeAuthFactor& fake_factor = label_factor_pair.second;
        return absl::get_if<FakeFactorType>(&fake_factor) != nullptr;
      });
  return it != std::end(factors);
}

bool AuthInputMatchesFakeFactorType(
    const ::user_data_auth::AuthInput& auth_input,
    const FakeAuthFactor& fake_factor) {
  return absl::visit(
      Overload<bool>(
          [&](const PasswordFactor& password) {
            return auth_input.has_password_input();
          },
          [&](const PinFactor& pin) { return auth_input.has_pin_input(); },
          [&](const RecoveryFactor& recovery) {
            return auth_input.has_cryptohome_recovery_input();
          },
          [&](const KioskFactor& kiosk) {
            return auth_input.has_kiosk_input();
          },
          [&](const SmartCardFactor& smart_card) {
            return auth_input.has_smart_card_input();
          }),
      fake_factor);
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), *reply_));
  }

  ReplyOnReturn<ReplyType>& operator=(const ReplyOnReturn<ReplyType>&) = delete;

 private:
  raw_ptr<ReplyType> reply_;
  chromeos::DBusMethodCallback<ReplyType> callback_;
};

}  // namespace

// =============== `AuthSessionData` =====================
FakeUserDataAuthClient::AuthSessionData::AuthSessionData() = default;
FakeUserDataAuthClient::AuthSessionData::AuthSessionData(
    const AuthSessionData& other) = default;
FakeUserDataAuthClient::AuthSessionData&
FakeUserDataAuthClient::AuthSessionData::operator=(const AuthSessionData&) =
    default;
FakeUserDataAuthClient::AuthSessionData::~AuthSessionData() = default;

// static
FakeUserDataAuthClient::TestApi* FakeUserDataAuthClient::TestApi::Get() {
  static TestApi instance;
  return &instance;
}

// static
void FakeUserDataAuthClient::TestApi::OverrideGlobalInstance(
    std::unique_ptr<FakeUserDataAuthClient> client) {
  CHECK(!g_instance);
  g_instance = client.release();
}

void FakeUserDataAuthClient::TestApi::SetServiceIsAvailable(bool is_available) {
  FakeUserDataAuthClient::Get()->service_is_available_ = is_available;
  if (!is_available) {
    return;
  }
  FakeUserDataAuthClient::Get()
      ->RunPendingWaitForServiceToBeAvailableCallbacks();
}

void FakeUserDataAuthClient::TestApi::ReportServiceIsNotAvailable() {
  DCHECK(!FakeUserDataAuthClient::Get()->service_is_available_);
  FakeUserDataAuthClient::Get()->service_reported_not_available_ = true;
  FakeUserDataAuthClient::Get()
      ->RunPendingWaitForServiceToBeAvailableCallbacks();
}

void FakeUserDataAuthClient::TestApi::SetHomeEncryptionMethod(
    const cryptohome::AccountIdentifier& cryptohome_id,
    HomeEncryptionMethod method) {
  auto user_it = FakeUserDataAuthClient::Get()->users_.find(cryptohome_id);
  if (user_it == std::end(FakeUserDataAuthClient::Get()->users_)) {
    LOG(ERROR) << "User does not exist: " << cryptohome_id.account_id();
    // TODO(crbug.com/1334538): Some existing tests rely on us creating the
    // user here, but new tests shouldn't. Eventually this should crash.
    user_it = FakeUserDataAuthClient::Get()
                  ->users_.insert({cryptohome_id, UserCryptohomeState()})
                  .first;
  }
  DCHECK(user_it != std::end(FakeUserDataAuthClient::Get()->users_));
  UserCryptohomeState& user_state = user_it->second;
  user_state.home_encryption_method = method;
}

void FakeUserDataAuthClient::TestApi::SetEncryptionMigrationIncomplete(
    const cryptohome::AccountIdentifier& cryptohome_id,
    bool incomplete) {
  auto user_it = FakeUserDataAuthClient::Get()->users_.find(cryptohome_id);
  if (user_it == std::end(FakeUserDataAuthClient::Get()->users_)) {
    LOG(ERROR) << "User does not exist: " << cryptohome_id.account_id();
    // TODO(crbug.com/1334538): Some existing tests rely on us creating the
    // user here, but new tests shouldn't. Eventually this should crash.
    user_it = FakeUserDataAuthClient::Get()
                  ->users_.insert({cryptohome_id, UserCryptohomeState()})
                  .first;
  }
  DCHECK(user_it != std::end(FakeUserDataAuthClient::Get()->users_));
  UserCryptohomeState& user_state = user_it->second;
  user_state.incomplete_migration = incomplete;
}

void FakeUserDataAuthClient::TestApi::SetPinLocked(
    const cryptohome::AccountIdentifier& account_id,
    const std::string& label,
    bool locked) {
  auto user_it = FakeUserDataAuthClient::Get()->users_.find(account_id);
  CHECK(user_it != FakeUserDataAuthClient::Get()->users_.end())
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
      FakeUserDataAuthClient::Get()->users_.insert(
          {std::move(account_id), UserCryptohomeState()});
  if (!was_inserted) {
    LOG(WARNING) << "User already exists: " << user_it->first.account_id();
    return;
  }

  const absl::optional<base::FilePath> profile_dir =
      FakeUserDataAuthClient::Get()->GetUserProfileDir(user_it->first);
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
  return FakeUserDataAuthClient::Get()->GetUserProfileDir(account_id);
}

void FakeUserDataAuthClient::TestApi::AddKey(
    const cryptohome::AccountIdentifier& account_id,
    const cryptohome::Key& key) {
  UserCryptohomeState& user_state = GetUserState(account_id);

  const auto [factor_it, was_inserted] =
      user_state.auth_factors.insert(KeyToFakeAuthFactor(
          key, FakeUserDataAuthClient::Get()->enable_auth_check_));
  CHECK(was_inserted) << "Factor already exists";
}

void FakeUserDataAuthClient::TestApi::AddRecoveryFactor(
    const cryptohome::AccountIdentifier& account_id) {
  UserCryptohomeState& user_state = GetUserState(account_id);

  FakeAuthFactor factor{RecoveryFactor()};
  const auto [factor_it, was_inserted] = user_state.auth_factors.insert(
      {kCryptohomeRecoveryKeyLabel, std::move(factor)});
  CHECK(was_inserted) << "Factor already exists";
}

bool FakeUserDataAuthClient::TestApi::HasRecoveryFactor(
    const cryptohome::AccountIdentifier& account_id) {
  const UserCryptohomeState& user_state = GetUserState(account_id);
  return ContainsFakeFactor<RecoveryFactor>(user_state.auth_factors);
}

bool FakeUserDataAuthClient::TestApi::HasPinFactor(
    const cryptohome::AccountIdentifier& account_id) {
  const UserCryptohomeState& user_state = GetUserState(account_id);
  return ContainsFakeFactor<PinFactor>(user_state.auth_factors);
}

std::string FakeUserDataAuthClient::TestApi::AddSession(
    const cryptohome::AccountIdentifier& account_id,
    bool authenticated) {
  CHECK(FakeUserDataAuthClient::Get()->users_.contains(account_id));

  std::string auth_session_id = base::StringPrintf(
      kAuthSessionIdTemplate,
      FakeUserDataAuthClient::Get()->next_auth_session_id_++);

  CHECK_EQ(FakeUserDataAuthClient::Get()->auth_sessions_.count(auth_session_id),
           0u);
  AuthSessionData& session =
      FakeUserDataAuthClient::Get()->auth_sessions_[auth_session_id];

  session.id = auth_session_id;
  session.ephemeral = false;
  session.account = account_id;
  session.authenticated = authenticated;

  return auth_session_id;
}

void FakeUserDataAuthClient::TestApi::DestroySessions() {
  g_instance->auth_sessions_.clear();
}

FakeUserDataAuthClient::UserCryptohomeState&
FakeUserDataAuthClient::TestApi::GetUserState(
    const cryptohome::AccountIdentifier& account_id) {
  const auto user_it = FakeUserDataAuthClient::Get()->users_.find(account_id);
  CHECK(user_it != std::end(FakeUserDataAuthClient::Get()->users_))
      << "User doesn't exist";
  return user_it->second;
}

void FakeUserDataAuthClient::TestApi::SendLegacyFPAuthSignal(
    user_data_auth::FingerprintScanResult result) {
  for (auto& observer : g_instance->fingerprint_observers_) {
    observer.OnFingerprintScan(result);
  }
}

FakeUserDataAuthClient::FakeUserDataAuthClient() = default;

FakeUserDataAuthClient::~FakeUserDataAuthClient() {
  if (this == g_instance) {
    // If we're deleting the global instance, clear the pointer to it.
    g_instance = nullptr;
  }
}

// static
FakeUserDataAuthClient* FakeUserDataAuthClient::Get() {
  if (!g_instance) {
    g_instance = new FakeUserDataAuthClient();
  }
  return g_instance;
}

void FakeUserDataAuthClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeUserDataAuthClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakeUserDataAuthClient::AddFingerprintAuthObserver(
    FingerprintAuthObserver* observer) {
  fingerprint_observers_.AddObserver(observer);
}

void FakeUserDataAuthClient::RemoveFingerprintAuthObserver(
    FingerprintAuthObserver* observer) {
  fingerprint_observers_.RemoveObserver(observer);
}

void FakeUserDataAuthClient::IsMounted(
    const ::user_data_auth::IsMountedRequest& request,
    IsMountedCallback callback) {
  ::user_data_auth::IsMountedReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  reply.set_is_mounted(true);
}

void FakeUserDataAuthClient::Unmount(
    const ::user_data_auth::UnmountRequest& request,
    UnmountCallback callback) {
  ::user_data_auth::UnmountReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
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

void FakeUserDataAuthClient::StartFingerprintAuthSession(
    const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
    StartFingerprintAuthSessionCallback callback) {
  ::user_data_auth::StartFingerprintAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
}
void FakeUserDataAuthClient::EndFingerprintAuthSession(
    const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
    EndFingerprintAuthSessionCallback callback) {
  ::user_data_auth::EndFingerprintAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
}
void FakeUserDataAuthClient::StartMigrateToDircrypto(
    const ::user_data_auth::StartMigrateToDircryptoRequest& request,
    StartMigrateToDircryptoCallback callback) {
  last_migrate_to_dircrypto_request_ = request;
  ::user_data_auth::StartMigrateToDircryptoReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  dircrypto_migration_progress_ = 0;

  if (run_default_dircrypto_migration_) {
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
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  reply.set_low_entropy_credentials_supported(
      supports_low_entropy_credentials_);
}
void FakeUserDataAuthClient::GetAccountDiskUsage(
    const ::user_data_auth::GetAccountDiskUsageRequest& request,
    GetAccountDiskUsageCallback callback) {
  ::user_data_auth::GetAccountDiskUsageReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  // Sets 100 MB as a fake usage.
  reply.set_size(100 * 1024 * 1024);
}

void FakeUserDataAuthClient::StartAuthSession(
    const ::user_data_auth::StartAuthSessionRequest& request,
    StartAuthSessionCallback callback) {
  ::user_data_auth::StartAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kStartAuthSession);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  std::string auth_session_id =
      base::StringPrintf(kAuthSessionIdTemplate, next_auth_session_id_++);

  DCHECK_EQ(auth_sessions_.count(auth_session_id), 0u);
  AuthSessionData& session = auth_sessions_[auth_session_id];
  session.id = auth_session_id;
  session.ephemeral =
      (request.flags() & ::user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER) !=
      0;
  session.account = request.account_id();
  session.requested_auth_session_intent = request.intent();

  reply.set_auth_session_id(auth_session_id);

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  reply.set_user_exists(user_exists);

  const std::string& account_id = request.account_id().account_id();
  // See device_local_account.h
  const bool is_kiosk =
      base::EndsWith(account_id, "kiosk-apps.device-local.localhost");

  if (user_exists) {
    UserCryptohomeState& user_state = user_it->second;

    // TODO(b/239422391): Some tests expect that kiosk or gaia keys exist
    // for existing users, but don't set those keys up. Until those tests are
    // fixed, we explicitly add keys here.
    if (is_kiosk) {
      if (!user_state.auth_factors.contains(kCryptohomePublicMountLabel)) {
        LOG(ERROR) << "Listing kiosk key even though it was not set up";

        FakeAuthFactor factor{KioskFactor()};
        user_state.auth_factors.insert(
            {kCryptohomeRecoveryKeyLabel, std::move(factor)});
      };
    } else {
      if (!user_state.auth_factors.contains(kCryptohomeGaiaKeyLabel)) {
        LOG(ERROR) << "Listing GAIA password key even though it was not set up";
        FakeAuthFactor factor{PasswordFactor()};
        user_state.auth_factors.insert(
            {kCryptohomeGaiaKeyLabel, std::move(factor)});
      };
    }

    for (const auto& [label, factor] : user_state.auth_factors) {
      absl::optional<cryptohome::KeyData> key_data =
          FakeAuthFactorToKeyData(label, factor);
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
}

void FakeUserDataAuthClient::ListAuthFactors(
    const ::user_data_auth::ListAuthFactorsRequest& request,
    ListAuthFactorsCallback callback) {
  ::user_data_auth::ListAuthFactorsReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kListAuthFactors);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  if (!user_exists) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }

  const UserCryptohomeState& user_state = user_it->second;
  for (const auto& [label, factor] : user_state.auth_factors) {
    absl::optional<user_data_auth::AuthFactor> auth_factor =
        FakeAuthFactorToAuthFactor(label, factor);
    if (auth_factor) {
      *reply.add_configured_auth_factors() = *auth_factor;
      // Hack as cryptohome sends information via list of available intents.
      auto* factor_with_status =
          reply.add_configured_auth_factors_with_status();
      *factor_with_status->mutable_auth_factor() = *auth_factor;
      factor_with_status->add_available_for_intents(
          user_data_auth::AUTH_INTENT_DECRYPT);
      factor_with_status->add_available_for_intents(
          user_data_auth::AUTH_INTENT_VERIFY_ONLY);
      factor_with_status->add_available_for_intents(
          user_data_auth::AUTH_INTENT_WEBAUTHN);
      if (absl::holds_alternative<PinFactor>(factor)) {
        if (absl::get<PinFactor>(factor).locked) {
          factor_with_status->clear_available_for_intents();
        }
      }
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
    if (supports_low_entropy_credentials_) {
      reply.add_supported_auth_factors(user_data_auth::AUTH_FACTOR_TYPE_PIN);
    }
    reply.add_supported_auth_factors(
        user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY);
  }
}

void FakeUserDataAuthClient::PrepareGuestVault(
    const ::user_data_auth::PrepareGuestVaultRequest& request,
    PrepareGuestVaultCallback callback) {
  ::user_data_auth::PrepareGuestVaultReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kPrepareGuestVault);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  prepare_guest_request_count_++;

  cryptohome::AccountIdentifier account;
  account.set_account_id(kGuestUserName);
  reply.set_sanitized_username(GetStubSanitizedUsername(account));
}

void FakeUserDataAuthClient::PrepareEphemeralVault(
    const ::user_data_auth::PrepareEphemeralVaultRequest& request,
    PrepareEphemeralVaultCallback callback) {
  ::user_data_auth::PrepareEphemeralVaultReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kPrepareEphemeralVault);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  AuthSessionData& auth_session = session_it->second;
  if (!auth_session.ephemeral) {
    LOG(ERROR) << "Non-ephemeral AuthSession used with PrepareEphemeralVault";
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }
  cryptohome::AccountIdentifier account = auth_session.account;
  // Ephemeral mount does not require session to be authenticated;
  // It authenticates session instead.
  if (auth_session.authenticated) {
    LOG(ERROR) << "AuthSession is authenticated";
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }
  auth_session.authenticated = true;

  const auto [_, was_inserted] =
      users_.insert({auth_session.account, UserCryptohomeState()});

  if (!was_inserted) {
    LOG(ERROR) << "User already exists: " << auth_session.account.account_id();
    reply.set_error(
        CryptohomeErrorCode::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
    return;
  }

  reply.set_sanitized_username(GetStubSanitizedUsername(account));
}

void FakeUserDataAuthClient::CreatePersistentUser(
    const ::user_data_auth::CreatePersistentUserRequest& request,
    CreatePersistentUserCallback callback) {
  ::user_data_auth::CreatePersistentUserReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kCreatePersistentUser);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  AuthSessionData& auth_session = session_it->second;

  if (auth_session.ephemeral) {
    LOG(ERROR) << "Ephemeral AuthSession used with CreatePersistentUser";
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }

  const auto [_, was_inserted] =
      users_.insert({auth_session.account, UserCryptohomeState()});

  if (!was_inserted) {
    LOG(ERROR) << "User already exists: " << auth_session.account.account_id();
    reply.set_error(
        CryptohomeErrorCode::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
    return;
  }

  auth_session.authenticated = true;
}

void FakeUserDataAuthClient::PreparePersistentVault(
    const ::user_data_auth::PreparePersistentVaultRequest& request,
    PreparePersistentVaultCallback callback) {
  ::user_data_auth::PreparePersistentVaultReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kPreparePersistentVault);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  auto error = CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    reply.set_error(error);
    return;
  }

  if (authenticated_auth_session->ephemeral) {
    LOG(ERROR) << "Ephemeral AuthSession used with PreparePersistentVault";
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return;
  }

  const auto user_it = users_.find(authenticated_auth_session->account);
  if (user_it == std::end(users_)) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }

  if (request.block_ecryptfs() && user_it->second.home_encryption_method ==
                                      HomeEncryptionMethod::kEcryptfs) {
    if (user_it->second.incomplete_migration) {
      LOG(ERROR) << "Encryption migration required, incomplete migration";
      reply.set_error(CryptohomeErrorCode::
                          CRYPTOHOME_ERROR_MOUNT_PREVIOUS_MIGRATION_INCOMPLETE);
    } else {
      LOG(ERROR) << "Encryption migration required, full migration";
      reply.set_error(
          CryptohomeErrorCode::CRYPTOHOME_ERROR_MOUNT_OLD_ENCRYPTION);
    }
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

  if (auto error = TakeOperationError(Operation::kPrepareVaultForMigration);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  auto error = CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    reply.set_error(error);
    return;
  }

  if (!users_.contains(authenticated_auth_session->account)) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND);
    return;
  }
}

void FakeUserDataAuthClient::InvalidateAuthSession(
    const ::user_data_auth::InvalidateAuthSessionRequest& request,
    InvalidateAuthSessionCallback callback) {
  ::user_data_auth::InvalidateAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto auth_session = auth_sessions_.find(request.auth_session_id());
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }

  auth_sessions_.erase(auth_session);
}

void FakeUserDataAuthClient::ExtendAuthSession(
    const ::user_data_auth::ExtendAuthSessionRequest& request,
    ExtendAuthSessionCallback callback) {
  ::user_data_auth::ExtendAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto error = CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);
}

void FakeUserDataAuthClient::AddAuthFactor(
    const ::user_data_auth::AddAuthFactorRequest& request,
    AddAuthFactorCallback callback) {
  ::user_data_auth::AddAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  last_add_auth_factor_request_ = request;

  if (auto error = TakeOperationError(Operation::kAddAuthFactor);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  auto error = CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  if (session == nullptr) {
    reply.set_error(error);
    return;
  }

  auto user_it = users_.find(session->account);
  CHECK(user_it != std::end(users_))
      << "User associated with session does not exist";
  UserCryptohomeState& user_state = user_it->second;

  auto [new_label, new_factor] = AuthFactorWithInputToFakeAuthFactor(
      request.auth_factor(), request.auth_input(), enable_auth_check_);
  CHECK(!user_state.auth_factors.contains(new_label))
      << "Key exists, will not clobber: " << new_label;
  user_state.auth_factors[std::move(new_label)] = std::move(new_factor);
}

void FakeUserDataAuthClient::AuthenticateAuthFactor(
    const ::user_data_auth::AuthenticateAuthFactorRequest& request,
    AuthenticateAuthFactorCallback callback) {
  ::user_data_auth::AuthenticateAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (auto error = TakeOperationError(Operation::kAuthenticateAuthFactor);
      error != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    reply.set_error(error);
    return;
  }

  last_unlock_webauthn_secret_ = false;

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  auto& session = session_it->second;

  CHECK(!session.authenticated) << "Session is already authenticated";

  const auto user_it = users_.find(session.account);
  DCHECK(user_it != std::end(users_));
  const UserCryptohomeState& user_state = user_it->second;

  const std::string& label = request.auth_factor_label();
  const auto factor_it = user_state.auth_factors.find(label);
  if (factor_it == user_state.auth_factors.end()) {
    LOG(ERROR) << "Factor not found: " << label;
    reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    return;
  }
  const FakeAuthFactor& factor = factor_it->second;

  const ::user_data_auth::AuthInput& auth_input = request.auth_input();

  if (!AuthInputMatchesFakeFactorType(auth_input, factor)) {
    LOG(ERROR) << "Auth input does not match factor type";
    reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
    return;
  }

  // Factor-specific verification logic. Will set the `error` field in the
  // reply if a check didn't pass.
  absl::visit(
      Overload<void>(
          [&](const PasswordFactor& password_factor) {
            const auto& password_input = auth_input.password_input();

            if (enable_auth_check_ &&
                password_input.secret() != password_factor.password) {
              reply.set_error(
                  ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
              return;
            }
          },
          [&](const PinFactor& pin_factor) {
            const auto& pin_input = auth_input.pin_input();

            if (enable_auth_check_ && pin_input.secret() != pin_factor.pin) {
              reply.set_error(
                  ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
              return;
            }
          },
          [&](const RecoveryFactor& recovery) {
            const auto& recovery_input = auth_input.cryptohome_recovery_input();

            if (recovery_input.mediator_pub_key().empty()) {
              LOG(ERROR) << "Missing mediate pub key";
              reply.set_error(
                  ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
              return;
            }
            if (recovery_input.epoch_response().empty()) {
              LOG(ERROR) << "Missing epoch response";
              reply.set_error(
                  ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
              return;
            }
            if (recovery_input.recovery_response().empty()) {
              LOG(ERROR) << "Missing recovery response";
              reply.set_error(
                  ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
              return;
            }
          },
          [&](const KioskFactor& kiosk) {},
          [&](const SmartCardFactor& smart_card) {
            LOG(ERROR) << "Checking smart card key is not implemented yet";
          }),
      factor);

  if (reply.error() != CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    return;
  }

  session.authenticated = true;
  session.authorized_auth_session_intent.Put(
      session.requested_auth_session_intent);
  if (session.requested_auth_session_intent ==
      user_data_auth::AUTH_INTENT_DECRYPT) {
    reply.set_authenticated(true);
  }
  reply.add_authorized_for(session.requested_auth_session_intent);
  reply.set_seconds_left(kSessionTimeoutSeconds);
}

void FakeUserDataAuthClient::UpdateAuthFactor(
    const ::user_data_auth::UpdateAuthFactorRequest& request,
    UpdateAuthFactorCallback callback) {
  ::user_data_auth::UpdateAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto error = CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);
  if (session == nullptr) {
    return;
  }

  auto user_it = users_.find(session->account);
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  // Update the fake auth factor according to the new secret.
  auto [new_label, new_factor] = AuthFactorWithInputToFakeAuthFactor(
      request.auth_factor(), request.auth_input(), enable_auth_check_);
  CHECK_EQ(new_label, request.auth_factor_label());
  CHECK(user_state.auth_factors.contains(new_label))
      << "Key does not exist: " << new_label;
  user_state.auth_factors[std::move(new_label)] = std::move(new_factor);
}

void FakeUserDataAuthClient::RemoveAuthFactor(
    const ::user_data_auth::RemoveAuthFactorRequest& request,
    RemoveAuthFactorCallback callback) {
  ::user_data_auth::RemoveAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto error = CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  reply.set_error(error);
  if (session == nullptr) {
    return;
  }
  auto user_it = users_.find(session->account);
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  const std::string& label = request.auth_factor_label();
  DCHECK(!label.empty());
  bool erased = user_state.auth_factors.erase(label) > 0;

  if (!erased) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  }
}

void FakeUserDataAuthClient::GetAuthFactorExtendedInfo(
    const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
    GetAuthFactorExtendedInfoCallback callback) {
  ::user_data_auth::GetAuthFactorExtendedInfoReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
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
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const std::string auth_session_id = request.auth_session_id();
  auto auth_session = auth_sessions_.find(auth_session_id);

  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }
  if (!auth_session->second.authenticated) {
    reply.set_status(
        ::user_data_auth::AUTH_SESSION_STATUS_FURTHER_FACTOR_REQUIRED);
    return;
  }
  reply.set_status(::user_data_auth::AUTH_SESSION_STATUS_AUTHENTICATED);
  // Use 5 minutes timeout - as if auth session has just started.
  reply.set_time_left(5 * 60);
}

void FakeUserDataAuthClient::PrepareAuthFactor(
    const ::user_data_auth::PrepareAuthFactorRequest& request,
    PrepareAuthFactorCallback callback) {
  ::user_data_auth::PrepareAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const std::string auth_session_id = request.auth_session_id();
  auto auth_session = auth_sessions_.find(auth_session_id);
  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }

  CHECK_EQ(request.auth_factor_type(),
           user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT)
      << "Only Legacy FP is supported in FakeUDAC";
  CHECK(!fingerprint_observers_.empty())
      << "Add relevant observer before calling PrepareAuthFactor";

  CHECK(!auth_session->second.is_listening_for_fingerprint_events)
      << "Duplicate call to PrepareAuthFactor";
  auth_session->second.is_listening_for_fingerprint_events = true;
}

void FakeUserDataAuthClient::TerminateAuthFactor(
    const ::user_data_auth::TerminateAuthFactorRequest& request,
    TerminateAuthFactorCallback callback) {
  ::user_data_auth::TerminateAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  const std::string auth_session_id = request.auth_session_id();
  auto auth_session = auth_sessions_.find(auth_session_id);
  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    reply.set_error(CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return;
  }

  CHECK_EQ(request.auth_factor_type(),
           user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT)
      << "Only Legacy FP is supported in FakeUDAC";

  CHECK(auth_session->second.is_listening_for_fingerprint_events)
      << "Call to TerminateAuthFactor without prior PrepareAuthFactor";
  auth_session->second.is_listening_for_fingerprint_events = false;
}

void FakeUserDataAuthClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  if (service_is_available_ || service_reported_not_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), service_is_available_));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(
        std::move(callback));
  }
}

void FakeUserDataAuthClient::RunPendingWaitForServiceToBeAvailableCallbacks() {
  std::vector<chromeos::WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(false);
  }
}

FakeUserDataAuthClient::AuthResult
FakeUserDataAuthClient::AuthenticateViaAuthFactors(
    const cryptohome::AccountIdentifier& account_id,
    const std::string& factor_label,
    const std::string& secret,
    bool wildcard_allowed,
    std::string* matched_factor_label) const {
  if (!enable_auth_check_) {
    return AuthResult::kAuthSuccess;
  }

  const auto user_it = users_.find(account_id);
  if (user_it == std::end(users_)) {
    return AuthResult::kUserNotFound;
  }
  const UserCryptohomeState& user_state = user_it->second;

  if (wildcard_allowed && factor_label.empty()) {
    // Do a wildcard match (it's only used for legacy APIs): try the secret
    // against every credential.
    for (const auto& [candidate_label, candidate_factor] :
         user_state.auth_factors) {
      if (CheckCredentialsViaAuthFactor(candidate_factor, secret)) {
        if (matched_factor_label) {
          *matched_factor_label = candidate_label;
        }
        return AuthResult::kAuthSuccess;
      }
    }
    // It's not well-defined which error is returned on a failed wildcard
    // authentication, but we follow what the real cryptohome does (at least in
    // CheckKey).
    return AuthResult::kAuthFailed;
  }

  const auto factor_it = user_state.auth_factors.find(factor_label);
  if (factor_it == std::end(user_state.auth_factors)) {
    return AuthResult::kFactorNotFound;
  }
  const auto& [label, factor] = *factor_it;
  if (!CheckCredentialsViaAuthFactor(factor, secret)) {
    return AuthResult::kAuthFailed;
  }
  if (matched_factor_label) {
    *matched_factor_label = label;
  }
  return AuthResult::kAuthSuccess;
}

void FakeUserDataAuthClient::SetNextOperationError(
    FakeUserDataAuthClient::Operation operation,
    CryptohomeErrorCode error) {
  operation_errors_[operation] = error;
}

CryptohomeErrorCode FakeUserDataAuthClient::TakeOperationError(
    Operation operation) {
  const auto op_error = operation_errors_.find(operation);
  if (op_error == std::end(operation_errors_)) {
    return CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  }
  CryptohomeErrorCode result = op_error->second;
  operation_errors_.erase(op_error);
  return result;
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
  for (auto& observer : observer_list_) {
    observer.LowDiskSpace(status);
  }
}

void FakeUserDataAuthClient::NotifyDircryptoMigrationProgress(
    ::user_data_auth::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  ::user_data_auth::DircryptoMigrationProgress progress;
  progress.set_status(status);
  progress.set_current_bytes(current);
  progress.set_total_bytes(total);
  for (auto& observer : observer_list_) {
    observer.DircryptoMigrationProgress(progress);
  }
}

absl::optional<base::FilePath> FakeUserDataAuthClient::GetUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) const {
  if (!user_data_dir_.has_value()) {
    return absl::nullopt;
  }

  std::string user_dir_base_name =
      kUserDataDirNamePrefix + account_id.account_id() + kUserDataDirNameSuffix;
  const base::FilePath profile_dir =
      user_data_dir_->Append(std::move(user_dir_base_name));
  return profile_dir;
}

const FakeUserDataAuthClient::AuthSessionData*
FakeUserDataAuthClient::GetAuthenticatedAuthSession(
    const std::string& auth_session_id,
    CryptohomeErrorCode* error) const {
  auto auth_session = auth_sessions_.find(auth_session_id);

  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    *error = CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN;
    return nullptr;
  }

  // Check if the AuthSession is properly authenticated.
  if (!auth_session->second.authenticated) {
    LOG(ERROR) << "AuthSession is not authenticated";
    *error = CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT;
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
