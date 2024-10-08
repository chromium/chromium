// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/recoverable_key_store.pb.h"
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
  // This will be `std::nullopt` if auth checking hasn't been activated.
  std::optional<std::string> password;
};

struct PinFactor {
  // This will be `std::nullopt` if auth checking hasn't been activated.
  std::optional<std::string> pin = std::nullopt;
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
constexpr char kUserDataDirNamePrefix[] = "u-";
constexpr char kUserDataDirNameSuffix[] = "-hash";

// Label of the recovery auth factor.
// Label of the kiosk auth factor.
constexpr char kCryptohomePublicMountLabel[] = "publicmount";

// Labels used of of various types of auth factors used by chrome. These must
// be kept in sync with the labels in cryptohome_key_constants.{cc,h}, which
// cannot be included into this file because that would result in circular
// dependencies.
constexpr char kCryptohomeRecoveryKeyLabel[] = "recovery";
constexpr char kCryptohomeGaiaKeyLabel[] = "gaia";

template <typename ReplyType>
void SetErrorWrapperToReply(ReplyType& reply, cryptohome::ErrorWrapper error) {
  reply.set_error(error.code());
}

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

  // If users are created in test constructor, UserDataDir is not available
  // yet, so actual directory creation needs to be postponed.
  bool postponed_directory_creation = false;

  // Is the user ephemeral.
  bool ephemeral = false;
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

// Helper that returns a AuthFactorWithStatus with default field values.
user_data_auth::AuthFactorWithStatus BuildDefaultAuthFactorWithStatus() {
  user_data_auth::AuthFactorWithStatus factor_with_status;
  // Add all possible intents for conveniences.
  factor_with_status.add_available_for_intents(
      user_data_auth::AUTH_INTENT_DECRYPT);
  factor_with_status.add_available_for_intents(
      user_data_auth::AUTH_INTENT_VERIFY_ONLY);
  factor_with_status.add_available_for_intents(
      user_data_auth::AUTH_INTENT_WEBAUTHN);
  factor_with_status.mutable_status_info()->set_time_available_in(0);
  factor_with_status.mutable_status_info()->set_time_expiring_in(
      std::numeric_limits<uint64_t>::max());
  return factor_with_status;
}

std::optional<user_data_auth::AuthFactorWithStatus>
FakeAuthFactorToAuthFactorWithStatus(std::string label,
                                     const FakeAuthFactor& factor) {
  return absl::visit(
      Overload<std::optional<user_data_auth::AuthFactorWithStatus>>(
          [&](const PasswordFactor& password) {
            user_data_auth::AuthFactorWithStatus result =
                BuildDefaultAuthFactorWithStatus();
            user_data_auth::AuthFactor* factor = result.mutable_auth_factor();
            factor->set_label(std::move(label));
            factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_PASSWORD);
            factor->mutable_password_metadata();
            return result;
          },
          [&](const PinFactor& pin) {
            user_data_auth::AuthFactorWithStatus result =
                BuildDefaultAuthFactorWithStatus();
            if (pin.locked) {
              result.mutable_status_info()->set_time_available_in(
                  std::numeric_limits<uint64_t>::max());
            }
            user_data_auth::AuthFactor* factor = result.mutable_auth_factor();
            factor->set_label(std::move(label));
            factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_PIN);
            factor->mutable_pin_metadata();
            return result;
          },
          [&](const RecoveryFactor&) {
            user_data_auth::AuthFactorWithStatus result =
                BuildDefaultAuthFactorWithStatus();
            user_data_auth::AuthFactor* factor = result.mutable_auth_factor();
            factor->set_label(std::move(label));
            factor->set_type(
                user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY);
            factor->mutable_cryptohome_recovery_metadata();
            return result;
          },
          [&](const KioskFactor& kiosk) {
            user_data_auth::AuthFactorWithStatus result =
                BuildDefaultAuthFactorWithStatus();
            user_data_auth::AuthFactor* factor = result.mutable_auth_factor();
            factor->set_label(std::move(label));
            factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_KIOSK);
            factor->mutable_kiosk_metadata();
            return result;
          },
          [&](const SmartCardFactor& smart_card) {
            user_data_auth::AuthFactorWithStatus result =
                BuildDefaultAuthFactorWithStatus();
            user_data_auth::AuthFactor* factor = result.mutable_auth_factor();
            factor->set_label(std::move(label));
            factor->set_type(user_data_auth::AUTH_FACTOR_TYPE_SMART_CARD);
            factor->mutable_smart_card_metadata()->set_public_key_spki_der(
                smart_card.public_key_spki_der);
            return result;
          }),
      factor);
}

std::optional<cryptohome::RecoverableKeyStore>
FakeAuthFactorToRecoverableKeyStore(const FakeAuthFactor& factor) {
  return absl::visit(
      Overload<std::optional<cryptohome::RecoverableKeyStore>>(
          [&](const PasswordFactor& password) {
            cryptohome::RecoverableKeyStore store;
            store.mutable_key_store_metadata()->set_knowledge_factor_type(
                cryptohome::KNOWLEDGE_FACTOR_TYPE_PASSWORD);
            store.mutable_wrapped_security_domain_key()->set_key_name(
                "security_domain_member_key_encrypted_locally");
            return store;
          },
          [&](const PinFactor& pin) {
            cryptohome::RecoverableKeyStore store;
            store.mutable_key_store_metadata()->set_knowledge_factor_type(
                cryptohome::KNOWLEDGE_FACTOR_TYPE_PIN);
            store.mutable_wrapped_security_domain_key()->set_key_name(
                "security_domain_member_key_encrypted_locally");
            return store;
          },
          [&](const auto&) { return std::nullopt; }),
      factor);
}

// Turns AuthFactor+AuthInput into a pair of label and FakeAuthFactor.
std::pair<std::string, FakeAuthFactor> AuthFactorWithInputToFakeAuthFactor(
    const user_data_auth::AuthFactor& factor,
    const user_data_auth::AuthInput& input,
    bool save_secret) {
  const std::string& label = factor.label();
  CHECK_NE(label, "") << "Key label must not be empty string";

  std::optional<std::string> secret = std::nullopt;
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
      NOTREACHED_IN_MIGRATION();
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
          [&](const RecoveryFactor& recovery) -> bool {
            LOG(FATAL) << "Checking recovery key is not allowed";
          },
          [&](const KioskFactor& kiosk) {
            // Kiosk key secrets are derived from app ids and don't leave
            // cryptohome, so there's nothing to check.
            return true;
          },
          [&](const SmartCardFactor& smart_card) -> bool {
            LOG(FATAL) << "Checking smart card key is not implemented yet";
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

user_data_auth::VaultEncryptionType HomeEncryptionMethodToVaultEncryptionType(
    const FakeUserDataAuthClient::HomeEncryptionMethod home_encryption) {
  switch (home_encryption) {
    case FakeUserDataAuthClient::HomeEncryptionMethod::kDirCrypto:
      return user_data_auth::CRYPTOHOME_VAULT_ENCRYPTION_FSCRYPT;
    case FakeUserDataAuthClient::HomeEncryptionMethod::kEcryptfs:
      return user_data_auth::CRYPTOHOME_VAULT_ENCRYPTION_ECRYPTFS;
    case FakeUserDataAuthClient::HomeEncryptionMethod::kDmCrypt:
      return user_data_auth::CRYPTOHOME_VAULT_ENCRYPTION_DMCRYPT;
  }
}
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

  const std::optional<base::FilePath> profile_dir =
      FakeUserDataAuthClient::Get()->GetUserProfileDir(user_it->first);
  if (!profile_dir) {
    LOG(WARNING) << "User data directory has not been set, will not create "
                    "user profile directory";
    user_it->second.postponed_directory_creation = true;
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::CreateDirectory(*profile_dir));
}

std::optional<base::FilePath>
FakeUserDataAuthClient::TestApi::GetUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) const {
  return FakeUserDataAuthClient::Get()->GetUserProfileDir(account_id);
}

void FakeUserDataAuthClient::TestApi::CreatePostponedDirectories() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  for (auto& user_it : FakeUserDataAuthClient::Get()->users_) {
    if (!user_it.second.postponed_directory_creation) {
      continue;
    }
    const std::optional<base::FilePath> profile_dir =
        FakeUserDataAuthClient::Get()->GetUserProfileDir(user_it.first);
    CHECK(profile_dir) << "User data directory has not been set";
    CHECK(base::CreateDirectory(*profile_dir));
  }
}

void FakeUserDataAuthClient::TestApi::AddAuthFactor(
    const cryptohome::AccountIdentifier& account_id,
    const user_data_auth::AuthFactor& factor,
    const user_data_auth::AuthInput& input) {
  UserCryptohomeState& user_state = GetUserState(account_id);

  const auto [factor_it, was_inserted] = user_state.auth_factors.insert(
      AuthFactorWithInputToFakeAuthFactor(factor, input, true));
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

std::pair<std::string, std::string> FakeUserDataAuthClient::TestApi::AddSession(
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
  session.broadcast_id = "b-" + auth_session_id;
  session.ephemeral = false;
  session.account = account_id;
  session.authenticated = authenticated;

  return {auth_session_id, session.broadcast_id};
}

bool FakeUserDataAuthClient::TestApi::IsAuthenticated(
    const cryptohome::AccountIdentifier& account_id) {
  CHECK(FakeUserDataAuthClient::Get()->users_.contains(account_id));

  auto& auth_sessions = FakeUserDataAuthClient::Get()->auth_sessions_;

  auto [auth_session_id, session] =
      *find_if(std::begin(auth_sessions), std::end(auth_sessions),
               [&account_id](auto session_entry) {
                 return session_entry.second.account == account_id;
               });

  return session.authenticated;
}

bool FakeUserDataAuthClient::TestApi::IsCurrentSessionEphemeral() {
  CHECK_EQ(FakeUserDataAuthClient::Get()->auth_sessions_.size(), 1u);
  return FakeUserDataAuthClient::Get()
      ->auth_sessions_.begin()
      ->second.ephemeral;
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

void FakeUserDataAuthClient::TestApi::SetNextOperationError(
    Operation operation,
    cryptohome::ErrorWrapper error) {
  FakeUserDataAuthClient::Get()->SetNextOperationError(operation, error);
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

void FakeUserDataAuthClient::AddPrepareAuthFactorProgressObserver(
    PrepareAuthFactorProgressObserver* observer) {
  progress_observers_.AddObserver(observer);
}

void FakeUserDataAuthClient::RemovePrepareAuthFactorProgressObserver(
    PrepareAuthFactorProgressObserver* observer) {
  progress_observers_.RemoveObserver(observer);
}

void FakeUserDataAuthClient::AddAuthFactorStatusUpdateObserver(
    AuthFactorStatusUpdateObserver* observer) {
  auth_factor_status_observer_list_.AddObserver(observer);
}

void FakeUserDataAuthClient::RemoveAuthFactorStatusUpdateObserver(
    AuthFactorStatusUpdateObserver* observer) {
  auth_factor_status_observer_list_.RemoveObserver(observer);
}

void FakeUserDataAuthClient::IsMounted(
    const ::user_data_auth::IsMountedRequest& request,
    IsMountedCallback callback) {
  ::user_data_auth::IsMountedReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  bool result;
  if (request.username().empty()) {
    result = !mounted_user_dirs_.empty();
  } else {
    result =
        mounted_user_dirs_.find(request.username()) != mounted_user_dirs_.end();
  }
  reply.set_is_mounted(result);
}

void FakeUserDataAuthClient::GetVaultProperties(
    const ::user_data_auth::GetVaultPropertiesRequest& request,
    GetVaultPropertiesCallback callback) {
  ::user_data_auth::GetVaultPropertiesReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  if (request.username().empty()) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   ::user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }
  cryptohome::AccountIdentifier account;
  account.set_account_id(request.username());
  const auto user_it = users_.find(account);

  // User does not exist, return an error.
  if (user_it != std::end(users_)) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   ::user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }

  // User is not mounted, return an error.
  if (mounted_user_dirs_.find(request.username()) == mounted_user_dirs_.end()) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   ::user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }

  const auto user_state = user_it->second;

  // If ephemeral, then return error.
  if (user_state.ephemeral) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   ::user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }

  reply.set_encryption_type(HomeEncryptionMethodToVaultEncryptionType(
      user_state.home_encryption_method));
}

void FakeUserDataAuthClient::Unmount(
    const ::user_data_auth::UnmountRequest& request,
    UnmountCallback callback) {
  ::user_data_auth::UnmountReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  mounted_user_dirs_.clear();
}

void FakeUserDataAuthClient::Remove(
    const ::user_data_auth::RemoveRequest& request,
    RemoveCallback callback) {
  RememberRequest<Operation::kRemove>(request);
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
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   ::user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND));
    return;
  }

  const std::optional<base::FilePath> profile_dir =
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

void FakeUserDataAuthClient::StartMigrateToDircrypto(
    const ::user_data_auth::StartMigrateToDircryptoRequest& request,
    StartMigrateToDircryptoCallback callback) {
  ::user_data_auth::StartMigrateToDircryptoReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kStartMigrateToDircrypto>(request);

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
  RememberRequest<Operation::kStartAuthSession>(request);

  if (auto error = TakeOperationError(Operation::kStartAuthSession);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  std::string auth_session_id =
      base::StringPrintf(kAuthSessionIdTemplate, next_auth_session_id_++);

  DCHECK_EQ(auth_sessions_.count(auth_session_id), 0u);
  AuthSessionData& session = auth_sessions_[auth_session_id];
  session.id = auth_session_id;
  session.broadcast_id = "b-" + auth_session_id;
  session.ephemeral = request.is_ephemeral_user();
  session.account = request.account_id();
  session.requested_auth_session_intent = request.intent();

  reply.set_auth_session_id(auth_session_id);
  reply.set_broadcast_id(session.broadcast_id);

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  reply.set_user_exists(user_exists);

  const std::string& account_id = request.account_id().account_id();
  // See device_local_account.h
  const bool is_kiosk =
      base::EndsWith(account_id, "kiosk-apps.device-local.localhost");

  if (user_exists) {
    UserCryptohomeState& user_state = user_it->second;

    // TODO(b/239422391): Some tests expect that kiosk keys or gaia keys exist
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
      if (add_default_password_factor_ && user_state.auth_factors.empty()) {
        LOG(ERROR) << "Listing GAIA password key even though it was not set up";
        FakeAuthFactor factor{PasswordFactor()};
        user_state.auth_factors.insert(
            {kCryptohomeGaiaKeyLabel, std::move(factor)});
      };
    }

    for (const auto& [label, factor] : user_state.auth_factors) {
      std::optional<user_data_auth::AuthFactorWithStatus>
          auth_factor_with_status =
              FakeAuthFactorToAuthFactorWithStatus(label, factor);
      DCHECK(auth_factor_with_status);
      *reply.add_auth_factors() = auth_factor_with_status->auth_factor();
      *reply.add_configured_auth_factors_with_status() =
          *auth_factor_with_status;
    }
  }
}

void FakeUserDataAuthClient::ListAuthFactors(
    const ::user_data_auth::ListAuthFactorsRequest& request,
    ListAuthFactorsCallback callback) {
  ::user_data_auth::ListAuthFactorsReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kListAuthFactors>(request);

  if (auto error = TakeOperationError(Operation::kListAuthFactors);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  if (!user_exists) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND));
    return;
  }

  const UserCryptohomeState& user_state = user_it->second;
  for (const auto& [label, factor] : user_state.auth_factors) {
    std::optional<user_data_auth::AuthFactorWithStatus>
        auth_factor_with_status =
            FakeAuthFactorToAuthFactorWithStatus(label, factor);
    if (auth_factor_with_status) {
      *reply.add_configured_auth_factors() =
          auth_factor_with_status->auth_factor();
      *reply.add_configured_auth_factors_with_status() =
          *auth_factor_with_status;
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
      reply.add_supported_auth_factors(
          user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY);
    }
  }
}

void FakeUserDataAuthClient::PrepareGuestVault(
    const ::user_data_auth::PrepareGuestVaultRequest& request,
    PrepareGuestVaultCallback callback) {
  ::user_data_auth::PrepareGuestVaultReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kPrepareGuestVault>(request);

  if (auto error = TakeOperationError(Operation::kPrepareGuestVault);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
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
  RememberRequest<Operation::kPrepareEphemeralVault>(request);

  if (auto error = TakeOperationError(Operation::kPrepareEphemeralVault);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }
  AuthSessionData& auth_session = session_it->second;
  if (!auth_session.ephemeral) {
    LOG(ERROR) << "Non-ephemeral AuthSession used with PrepareEphemeralVault";
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }
  cryptohome::AccountIdentifier account = auth_session.account;
  // Ephemeral mount does not require session to be authenticated;
  // It authenticates session instead.
  if (auth_session.authenticated) {
    LOG(ERROR) << "AuthSession is authenticated";
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }
  auth_session.authenticated = true;
  auth_session.requested_auth_session_intent =
      user_data_auth::AUTH_INTENT_DECRYPT;
  auth_session.lifetime =
      base::Time::Now() + cryptohome::kAuthsessionInitialLifetime;

  auto user_state = UserCryptohomeState();
  user_state.ephemeral = true;
  const auto [_, was_inserted] =
      users_.insert({auth_session.account, user_state});

  if (!was_inserted) {
    LOG(ERROR) << "User already exists: " << auth_session.account.account_id();
    reply.set_error(
        CryptohomeErrorCode::CRYPTOHOME_ERROR_MOUNT_MOUNT_POINT_BUSY);
    return;
  }

  reply.set_sanitized_username(GetStubSanitizedUsername(account));

  reply.mutable_auth_properties()->add_authorized_for(
      auth_session.requested_auth_session_intent);
  reply.mutable_auth_properties()->set_seconds_left(
      cryptohome::kAuthsessionInitialLifetime.InSeconds());
}

void FakeUserDataAuthClient::CreatePersistentUser(
    const ::user_data_auth::CreatePersistentUserRequest& request,
    CreatePersistentUserCallback callback) {
  ::user_data_auth::CreatePersistentUserReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kCreatePersistentUser>(request);

  if (auto error = TakeOperationError(Operation::kCreatePersistentUser);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    reply.set_sanitized_username(std::string());
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }
  AuthSessionData& auth_session = session_it->second;

  if (auth_session.ephemeral) {
    LOG(ERROR) << "Ephemeral AuthSession used with CreatePersistentUser";
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
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
  auth_session.requested_auth_session_intent =
      user_data_auth::AUTH_INTENT_DECRYPT;
  auth_session.lifetime =
      base::Time::Now() + cryptohome::kAuthsessionInitialLifetime;

  reply.mutable_auth_properties()->add_authorized_for(
      auth_session.requested_auth_session_intent);
  reply.mutable_auth_properties()->set_seconds_left(
      cryptohome::kAuthsessionInitialLifetime.InSeconds());
}

void FakeUserDataAuthClient::PreparePersistentVault(
    const ::user_data_auth::PreparePersistentVaultRequest& request,
    PreparePersistentVaultCallback callback) {
  ::user_data_auth::PreparePersistentVaultReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kPreparePersistentVault>(request);

  if (auto error = TakeOperationError(Operation::kPreparePersistentVault);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  auto error = cryptohome::ErrorWrapper::success();
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  if (authenticated_auth_session->ephemeral) {
    LOG(ERROR) << "Ephemeral AuthSession used with PreparePersistentVault";
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
    return;
  }

  const auto user_it = users_.find(authenticated_auth_session->account);
  if (user_it == std::end(users_)) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND));
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
  mounted_user_dirs_.insert(authenticated_auth_session->account.account_id());
}

void FakeUserDataAuthClient::PrepareVaultForMigration(
    const ::user_data_auth::PrepareVaultForMigrationRequest& request,
    PrepareVaultForMigrationCallback callback) {
  ::user_data_auth::PrepareVaultForMigrationReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kPrepareVaultForMigration>(request);

  if (auto error = TakeOperationError(Operation::kPrepareVaultForMigration);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  auto error = cryptohome::ErrorWrapper::success();
  auto* authenticated_auth_session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);

  if (authenticated_auth_session == nullptr) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  if (!users_.contains(authenticated_auth_session->account)) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND));
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
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }

  auth_sessions_.erase(auth_session);
}

void FakeUserDataAuthClient::ExtendAuthSession(
    const ::user_data_auth::ExtendAuthSessionRequest& request,
    ExtendAuthSessionCallback callback) {
  ::user_data_auth::ExtendAuthSessionReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  auto error = cryptohome::ErrorWrapper::success();
  auto* session_data =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  SetErrorWrapperToReply(reply, error);
  if (session_data) {
    auth_sessions_.find(request.auth_session_id())->second.lifetime =
        base::Time::Now() + base::Seconds(request.extension_duration());
  }
}

void FakeUserDataAuthClient::AddAuthFactor(
    const ::user_data_auth::AddAuthFactorRequest& request,
    AddAuthFactorCallback callback) {
  ::user_data_auth::AddAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kAddAuthFactor>(request);

  if (auto error = TakeOperationError(Operation::kAddAuthFactor);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  auto error = cryptohome::ErrorWrapper::success();
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  if (session == nullptr) {
    SetErrorWrapperToReply(reply, error);
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
  RememberRequest<Operation::kAuthenticateAuthFactor>(request);

  if (auto error = TakeOperationError(Operation::kAuthenticateAuthFactor);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  const auto session_it = auth_sessions_.find(request.auth_session_id());
  if (session_it == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }
  auto& session = session_it->second;

  CHECK(!session.authenticated) << "Session is already authenticated";

  const auto user_it = users_.find(session.account);
  DCHECK(user_it != std::end(users_));
  const UserCryptohomeState& user_state = user_it->second;

  std::vector<std::string> auth_factor_labels;
  auth_factor_labels.reserve(request.auth_factor_labels_size());
  for (auto label : request.auth_factor_labels()) {
    auth_factor_labels.push_back(label);
  }

  const ::user_data_auth::AuthInput& auth_input = request.auth_input();

  // Checks that the arity of auth factor labels match the AuthInput type.
  // Legacy fingerprint does not have any auth factor associated.
  // And only sign-in fingerprint allows more than 1 auth factor label.
  if (auth_factor_labels.size() == 0) {
    if (!auth_input.has_legacy_fingerprint_input()) {
      SetErrorWrapperToReply(
          reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     ::user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
      return;
    }
  } else if (auth_factor_labels.size() > 1) {
    if (!auth_input.has_fingerprint_input()) {
      SetErrorWrapperToReply(
          reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     ::user_data_auth::CRYPTOHOME_ERROR_INVALID_ARGUMENT));
      return;
    }
  }

  for (const auto& label : auth_factor_labels) {
    const auto factor_it = user_state.auth_factors.find(label);
    if (factor_it == user_state.auth_factors.end()) {
      LOG(ERROR) << "Factor not found: " << label;
      SetErrorWrapperToReply(
          reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     ::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND));
      return;
    }
    const FakeAuthFactor& factor = factor_it->second;

    if (!AuthInputMatchesFakeFactorType(auth_input, factor)) {
      LOG(ERROR) << "Auth input does not match factor type";
      SetErrorWrapperToReply(
          reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     ::user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND));
      return;
    }

    // Factor-specific verification logic. Will set the result_error variable
    // variable if a check didn't pass.
    cryptohome::ErrorWrapper result_error = cryptohome::ErrorWrapper::success();
    absl::visit(
        Overload<void>(
            [&](const PasswordFactor& password_factor) {
              const auto& password_input = auth_input.password_input();

              if (enable_auth_check_ &&
                  password_input.secret() != password_factor.password) {
                result_error =
                    cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                        ::user_data_auth::
                            CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
                return;
              }
            },
            [&](const PinFactor& pin_factor) {
              const auto& pin_input = auth_input.pin_input();

              if (enable_auth_check_ && pin_input.secret() != pin_factor.pin) {
                result_error =
                    cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                        ::user_data_auth::
                            CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
                return;
              }

              if (pin_factor.locked) {
                result_error =
                    cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                        ::user_data_auth::CRYPTOHOME_ERROR_TPM_DEFEND_LOCK);
                return;
              }
            },
            [&](const RecoveryFactor& recovery) {
              const auto& recovery_input =
                  auth_input.cryptohome_recovery_input();

              if (recovery_input.epoch_response().empty()) {
                LOG(ERROR) << "Missing epoch response";
                result_error =
                    cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                        ::user_data_auth::
                            CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
                return;
              }
              if (recovery_input.recovery_response().empty()) {
                LOG(ERROR) << "Missing recovery response";
                result_error =
                    cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                        ::user_data_auth::
                            CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
                return;
              }
            },
            [&](const KioskFactor& kiosk) {},
            [&](const SmartCardFactor& smart_card) {
              LOG(ERROR) << "Checking smart card key is not implemented yet";
            }),
        factor);

    if (cryptohome::HasError(result_error)) {
      SetErrorWrapperToReply(reply, result_error);
      return;
    }
  }

  session.authenticated = true;
  session.authorized_auth_session_intent.Put(
      session.requested_auth_session_intent);
  session.lifetime =
      base::Time::Now() + cryptohome::kAuthsessionInitialLifetime;
  reply.mutable_auth_properties()->add_authorized_for(
      session.requested_auth_session_intent);
  reply.mutable_auth_properties()->set_seconds_left(
      cryptohome::kAuthsessionInitialLifetime.InSeconds());
}

void FakeUserDataAuthClient::UpdateAuthFactor(
    const ::user_data_auth::UpdateAuthFactorRequest& request,
    UpdateAuthFactorCallback callback) {
  ::user_data_auth::UpdateAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kUpdateAuthFactor>(request);

  if (auto error = TakeOperationError(Operation::kUpdateAuthFactor);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  auto error = cryptohome::ErrorWrapper::success();
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  SetErrorWrapperToReply(reply, error);
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

void FakeUserDataAuthClient::UpdateAuthFactorMetadata(
    const ::user_data_auth::UpdateAuthFactorMetadataRequest& request,
    UpdateAuthFactorMetadataCallback callback) {
  ::user_data_auth::UpdateAuthFactorMetadataReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kUpdateAuthFactorMetadata>(request);

  if (auto error = TakeOperationError(Operation::kUpdateAuthFactor);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  auto error = cryptohome::ErrorWrapper::success();
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  SetErrorWrapperToReply(reply, error);
  if (session == nullptr) {
    return;
  }

  auto user_it = users_.find(session->account);
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  // Check that the factor updated exists. We don't have to modify the stored
  // factor because metadata fields are not recorded in this implementation.
  CHECK(user_state.auth_factors.contains(request.auth_factor_label()))
      << "Key does not exist: " << request.auth_factor_label();
  return;
}

void FakeUserDataAuthClient::ReplaceAuthFactor(
    const ::user_data_auth::ReplaceAuthFactorRequest& request,
    ReplaceAuthFactorCallback callback) {
  ::user_data_auth::ReplaceAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kReplaceAuthFactor>(request);

  if (auto error = TakeOperationError(Operation::kReplaceAuthFactor);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  auto error = cryptohome::ErrorWrapper::success();
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  SetErrorWrapperToReply(reply, error);
  if (session == nullptr) {
    return;
  }

  auto user_it = users_.find(session->account);
  DCHECK(user_it != std::end(users_));
  UserCryptohomeState& user_state = user_it->second;

  const std::string& old_label = request.auth_factor_label();
  DCHECK(!old_label.empty());
  CHECK(user_state.auth_factors.contains(old_label))
      << "Key does not exist: " << old_label;

  auto [new_label, new_factor] = AuthFactorWithInputToFakeAuthFactor(
      request.auth_factor(), request.auth_input(), enable_auth_check_);
  CHECK(!user_state.auth_factors.contains(new_label))
      << "Key already exists: " << new_label;

  // Remove the fake old auth factor and replace with the new one.
  bool erased = user_state.auth_factors.erase(old_label) > 0;
  if (erased) {
    user_state.auth_factors[std::move(new_label)] = std::move(new_factor);
  } else {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND));
  }
}

void FakeUserDataAuthClient::RemoveAuthFactor(
    const ::user_data_auth::RemoveAuthFactorRequest& request,
    RemoveAuthFactorCallback callback) {
  ::user_data_auth::RemoveAuthFactorReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));

  auto error = cryptohome::ErrorWrapper::success();
  auto* session =
      GetAuthenticatedAuthSession(request.auth_session_id(), &error);
  SetErrorWrapperToReply(reply, error);
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
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND));
  }
}

void FakeUserDataAuthClient::GetAuthFactorExtendedInfo(
    const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
    GetAuthFactorExtendedInfoCallback callback) {
  ::user_data_auth::GetAuthFactorExtendedInfoReply reply;
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
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }

  base::TimeDelta time_left = auth_session->second.lifetime - base::Time::Now();
  reply.mutable_auth_properties()->add_authorized_for(
      auth_session->second.requested_auth_session_intent);
  reply.mutable_auth_properties()->set_seconds_left(time_left.InSeconds());
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
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }

  switch (request.auth_factor_type()) {
    case user_data_auth::AUTH_FACTOR_TYPE_CRYPTOHOME_RECOVERY:
      SetErrorWrapperToReply(
          reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                     CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET));
      reply.mutable_prepare_output()
          ->mutable_cryptohome_recovery_output()
          ->set_recovery_request("fake-recovery-request");
      break;
    case user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT:
      CHECK(!fingerprint_observers_.empty())
          << "Add relevant observer before calling PrepareAuthFactor";

      CHECK(!auth_session->second.is_listening_for_fingerprint_events)
          << "Duplicate call to PrepareAuthFactor";
      auth_session->second.is_listening_for_fingerprint_events = true;
      break;
    default:
      LOG(FATAL) << "Only Recovery and Legacy FP support PrepareAuthFactor in "
                    "FakeUDAC";
  }
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
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN));
    return;
  }

  CHECK_EQ(request.auth_factor_type(),
           user_data_auth::AUTH_FACTOR_TYPE_LEGACY_FINGERPRINT)
      << "Only Legacy FP is supported in FakeUDAC";

  CHECK(auth_session->second.is_listening_for_fingerprint_events)
      << "Call to TerminateAuthFactor without prior PrepareAuthFactor";
  auth_session->second.is_listening_for_fingerprint_events = false;
}

void FakeUserDataAuthClient::GetArcDiskFeatures(
    const ::user_data_auth::GetArcDiskFeaturesRequest& request,
    GetArcDiskFeaturesCallback callback) {
  ::user_data_auth::GetArcDiskFeaturesReply reply;
  reply.set_quota_supported(arc_quota_supported_);
  std::move(callback).Run(std::move(reply));
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
    cryptohome::ErrorWrapper error) {
  operation_errors_.insert_or_assign(operation, std::move(error));
  // operation_errors_[operation] = std::move(error);
}

cryptohome::ErrorWrapper FakeUserDataAuthClient::TakeOperationError(
    Operation operation) {
  const auto op_error = operation_errors_.find(operation);
  if (op_error == std::end(operation_errors_)) {
    return cryptohome::ErrorWrapper::success();
  }
  cryptohome::ErrorWrapper result = op_error->second;
  operation_errors_.erase(op_error);
  return result;
}

void FakeUserDataAuthClient::OnDircryptoMigrationProgressUpdated() {
  dircrypto_migration_progress_++;

  if (dircrypto_migration_progress_ >= kDircryptoMigrationMaxProgress) {
    NotifyDircryptoMigrationProgress(
        ::user_data_auth::DircryptoMigrationStatus::DIRCRYPTO_MIGRATION_SUCCESS,
        dircrypto_migration_progress_, kDircryptoMigrationMaxProgress);
    const auto user_it = users_.find(
        GetLastRequest<Operation::kStartMigrateToDircrypto>().account_id());
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

std::optional<base::FilePath> FakeUserDataAuthClient::GetUserProfileDir(
    const cryptohome::AccountIdentifier& account_id) const {
  if (!user_data_dir_.has_value()) {
    return std::nullopt;
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
    cryptohome::ErrorWrapper* error) const {
  auto auth_session = auth_sessions_.find(auth_session_id);

  // Check if the token refers to a valid AuthSession.
  if (auth_session == auth_sessions_.end()) {
    LOG(ERROR) << "AuthSession not found";
    *error = cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
        CryptohomeErrorCode::CRYPTOHOME_INVALID_AUTH_SESSION_TOKEN);
    return nullptr;
  }

  // Check if the AuthSession is properly authenticated.
  if (!auth_session->second.authenticated) {
    LOG(ERROR) << "AuthSession is not authenticated";
    *error = cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
        CryptohomeErrorCode::CRYPTOHOME_ERROR_INVALID_ARGUMENT);
    return nullptr;
  }

  return &auth_session->second;
}

void FakeUserDataAuthClient::SetUserDataDir(base::FilePath path) {
  CHECK(!user_data_dir_.has_value());
  user_data_dir_ = std::move(path);

  std::string pattern =
      base::StrCat({kUserDataDirNamePrefix, "*", kUserDataDirNameSuffix});
  base::FileEnumerator e(*user_data_dir_, /*recursive=*/false,
                         base::FileEnumerator::DIRECTORIES, std::move(pattern));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    const base::FilePath base_name = name.BaseName();
    DCHECK(base::StartsWith(base_name.value(), kUserDataDirNamePrefix));
    DCHECK(base::EndsWith(base_name.value(), kUserDataDirNameSuffix));

    // Remove kUserDataDirNamePrefix from front and kUserDataDirNameSuffix from
    // end to obtain account id.
    std::string account_id_str(
        base_name.value().begin() + std::strlen(kUserDataDirNamePrefix),
        base_name.value().end() - std::strlen(kUserDataDirNameSuffix));

    cryptohome::AccountIdentifier account_id;
    account_id.set_account_id(std::move(account_id_str));

    // This does intentionally not override existing entries.
    users_.insert({std::move(account_id), UserCryptohomeState()});
  }
}

void FakeUserDataAuthClient::GetRecoverableKeyStores(
    const ::user_data_auth::GetRecoverableKeyStoresRequest& request,
    GetRecoverableKeyStoresCallback callback) {
  ::user_data_auth::GetRecoverableKeyStoresReply reply;
  ReplyOnReturn auto_reply(&reply, std::move(callback));
  RememberRequest<Operation::kGetRecoverableKeyStores>(request);

  if (auto error = TakeOperationError(Operation::kGetRecoverableKeyStores);
      cryptohome::HasError(error)) {
    SetErrorWrapperToReply(reply, error);
    return;
  }

  const auto user_it = users_.find(request.account_id());
  const bool user_exists = user_it != std::end(users_);
  if (!user_exists) {
    SetErrorWrapperToReply(
        reply, cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
                   CryptohomeErrorCode::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND));
    return;
  }

  const UserCryptohomeState& user_state = user_it->second;
  for (const auto& [label, factor] : user_state.auth_factors) {
    std::optional<cryptohome::RecoverableKeyStore> store =
        FakeAuthFactorToRecoverableKeyStore(factor);
    if (store) {
      *reply.add_key_stores() = std::move(*store);
    }
  }
}

void FakeUserDataAuthClient::SetUserDataStorageWriteEnabled(
    const ::user_data_auth::SetUserDataStorageWriteEnabledRequest& request,
    SetUserDataStorageWriteEnabledCallback callback) {
  ::user_data_auth::SetUserDataStorageWriteEnabledReply reply;
  std::move(callback).Run(std::move(reply));
}

}  // namespace ash
