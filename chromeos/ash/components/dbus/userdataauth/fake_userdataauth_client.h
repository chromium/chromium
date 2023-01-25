// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/account_identifier_operators.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeUserDataAuthClient
    : public UserDataAuthClient {
 private:
  struct UserCryptohomeState;

 public:
  enum class Operation {
    kStartAuthSession,
    kAuthenticateAuthFactor,
    kPrepareGuestVault,
    kPrepareEphemeralVault,
    kCreatePersistentUser,
    kPreparePersistentVault,
    kPrepareVaultForMigration,
    kAddAuthFactor,
    kListAuthFactors,
  };

  // The method by which a user's home directory can be encrypted.
  enum class HomeEncryptionMethod {
    kDirCrypto,
    kEcryptfs,
  };

  // The TestAPI of FakeUserDataAuth. Prefer to use `ash::CryptohomeMixin`,
  // which exposes all the methods here and some additional ones.
  class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) TestApi {
   public:
    // Legacy method for tests that do not use `CryptohomeMixin`.
    static TestApi* Get();

    // Override the global fake instance for browser tests. Must be called
    // before browser startup, for example in the constructor of the fixture,
    // and before the global instance is configured in any way using this
    // TestApi or FakeUserDataAuth::Get().
    static void OverrideGlobalInstance(std::unique_ptr<FakeUserDataAuthClient>);

    // Sets whether dircrypto migration update should be run automatically.
    // If set to false, the client will not send any dircrypto migration
    // progress updates on its own - a test that sets this will have to call
    // NotifyDircryptoMigrationProgress() for the progress to update.
    void set_run_default_dircrypto_migration(bool value) {
      FakeUserDataAuthClient::Get()->run_default_dircrypto_migration_ = value;
    }

    // If set, next call to GetSupportedKeyPolicies() will tell caller that low
    // entropy credentials are supported.
    void set_supports_low_entropy_credentials(bool supports) {
      FakeUserDataAuthClient::Get()->supports_low_entropy_credentials_ =
          supports;
    }

    // If enable_auth_check is true, then authentication requests actually check
    // the key.
    void set_enable_auth_check(bool enable_auth_check) {
      FakeUserDataAuthClient::Get()->enable_auth_check_ = enable_auth_check;
    }

    // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
    // pending callbacks if is_available is true.
    void SetServiceIsAvailable(bool is_available);

    // Runs pending availability callbacks reporting that the service is
    // unavailable. Expects service not to be available when called.
    void ReportServiceIsNotAvailable();

    // Marks |cryptohome_id| as using ecryptfs (|use_ecryptfs|=true) or
    // dircrypto
    // (|use_ecryptfs|=false).
    void SetHomeEncryptionMethod(
        const cryptohome::AccountIdentifier& cryptohome_id,
        HomeEncryptionMethod method);

    // Marks |cryptohome_id| as failed previous migration attempt.
    void SetEncryptionMigrationIncomplete(
        const cryptohome::AccountIdentifier& cryptohome_id,
        bool incomplete);

    // Marks a PIN key as locked or unlocked. The key is identified by the
    // |account_id| of the user it belongs to and its |label|. The key must
    // exist prior to this call, and it must be a PIN key.
    void SetPinLocked(const cryptohome::AccountIdentifier& account_id,
                      const std::string& label,
                      bool locked);

    // Marks a user as existing and creates the user's home directory. No auth
    // factors are added.
    void AddExistingUser(const cryptohome::AccountIdentifier& account_id);

    // Returns the user's home directory, or an empty optional if the user data
    // directory is not initialized or the user doesn't exist.
    absl::optional<base::FilePath> GetUserProfileDir(
        const cryptohome::AccountIdentifier& account_id) const;

    // Adds the given key as a fake auth factor to the user (the user must
    // already exist).
    void AddKey(const cryptohome::AccountIdentifier& account_id,
                const cryptohome::Key& key);

    void AddRecoveryFactor(const cryptohome::AccountIdentifier& account_id);
    bool HasRecoveryFactor(const cryptohome::AccountIdentifier& account_id);

    bool HasPinFactor(const cryptohome::AccountIdentifier& account_id);

    std::string AddSession(const cryptohome::AccountIdentifier& account_id,
                           bool authenticated);

    void DestroySessions();

    void SendLegacyFPAuthSignal(user_data_auth::FingerprintScanResult result);

   private:
    FakeUserDataAuthClient::UserCryptohomeState& GetUserState(
        const cryptohome::AccountIdentifier& account_id);
  };

  // Represents the ongoing AuthSessions.
  struct AuthSessionData {
    explicit AuthSessionData();
    AuthSessionData(const AuthSessionData& other);
    AuthSessionData& operator=(const AuthSessionData&);
    ~AuthSessionData();
    // AuthSession id.
    std::string id;
    // Whether the `AUTH_SESSION_FLAGS_EPHEMERAL_USER` flag was passed on
    // creation.
    bool ephemeral = false;
    // Account associated with the session.
    cryptohome::AccountIdentifier account;
    // True if session is authenticated.
    bool authenticated = false;
    // The requested AuthIntent.
    user_data_auth::AuthIntent requested_auth_session_intent =
        user_data_auth::AUTH_INTENT_DECRYPT;

    using AuthProtoIntents =
        base::EnumSet<user_data_auth::AuthIntent,
                      user_data_auth::AuthIntent::AUTH_INTENT_UNSPECIFIED,
                      user_data_auth::AuthIntent::AUTH_INTENT_WEBAUTHN>;
    // List of Authorized AuthIntents.
    AuthProtoIntents authorized_auth_session_intent;

    // Indication that session is set to listen for FP events.
    bool is_listening_for_fingerprint_events = false;
  };

  FakeUserDataAuthClient();
  ~FakeUserDataAuthClient() override;

  // Not copyable or movable.
  FakeUserDataAuthClient(const FakeUserDataAuthClient&) = delete;
  FakeUserDataAuthClient& operator=(const FakeUserDataAuthClient&) = delete;

  // Checks that a FakeUserDataAuthClient instance was initialized and returns
  // it.
  static FakeUserDataAuthClient* Get();

  // UserDataAuthClient override:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void AddFingerprintAuthObserver(FingerprintAuthObserver* observer) override;
  void RemoveFingerprintAuthObserver(
      FingerprintAuthObserver* observer) override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                 IsMountedCallback callback) override;
  void Unmount(const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback) override;
  void Remove(const ::user_data_auth::RemoveRequest& request,
              RemoveCallback callback) override;
  void CheckKey(const ::user_data_auth::CheckKeyRequest& request,
                CheckKeyCallback callback) override;
  void StartFingerprintAuthSession(
      const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
      StartFingerprintAuthSessionCallback callback) override;
  void EndFingerprintAuthSession(
      const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
      EndFingerprintAuthSessionCallback callback) override;
  void StartMigrateToDircrypto(
      const ::user_data_auth::StartMigrateToDircryptoRequest& request,
      StartMigrateToDircryptoCallback callback) override;
  void NeedsDircryptoMigration(
      const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
      NeedsDircryptoMigrationCallback callback) override;
  void GetSupportedKeyPolicies(
      const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
      GetSupportedKeyPoliciesCallback callback) override;
  void GetAccountDiskUsage(
      const ::user_data_auth::GetAccountDiskUsageRequest& request,
      GetAccountDiskUsageCallback callback) override;
  void StartAuthSession(
      const ::user_data_auth::StartAuthSessionRequest& request,
      StartAuthSessionCallback callback) override;
  void PrepareGuestVault(
      const ::user_data_auth::PrepareGuestVaultRequest& request,
      PrepareGuestVaultCallback callback) override;
  void PrepareEphemeralVault(
      const ::user_data_auth::PrepareEphemeralVaultRequest& request,
      PrepareEphemeralVaultCallback callback) override;
  void CreatePersistentUser(
      const ::user_data_auth::CreatePersistentUserRequest& request,
      CreatePersistentUserCallback callback) override;
  void PreparePersistentVault(
      const ::user_data_auth::PreparePersistentVaultRequest& request,
      PreparePersistentVaultCallback callback) override;
  void PrepareVaultForMigration(
      const ::user_data_auth::PrepareVaultForMigrationRequest& request,
      PrepareVaultForMigrationCallback callback) override;
  void InvalidateAuthSession(
      const ::user_data_auth::InvalidateAuthSessionRequest& request,
      InvalidateAuthSessionCallback callback) override;
  void ExtendAuthSession(
      const ::user_data_auth::ExtendAuthSessionRequest& request,
      ExtendAuthSessionCallback callback) override;
  void AddAuthFactor(const ::user_data_auth::AddAuthFactorRequest& request,
                     AddAuthFactorCallback callback) override;
  void AuthenticateAuthFactor(
      const ::user_data_auth::AuthenticateAuthFactorRequest& request,
      AuthenticateAuthFactorCallback callback) override;
  void UpdateAuthFactor(
      const ::user_data_auth::UpdateAuthFactorRequest& request,
      UpdateAuthFactorCallback callback) override;
  void RemoveAuthFactor(
      const ::user_data_auth::RemoveAuthFactorRequest& request,
      RemoveAuthFactorCallback callback) override;
  void ListAuthFactors(const ::user_data_auth::ListAuthFactorsRequest& request,
                       ListAuthFactorsCallback callback) override;
  void GetAuthFactorExtendedInfo(
      const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
      GetAuthFactorExtendedInfoCallback callback) override;
  void GetRecoveryRequest(
      const ::user_data_auth::GetRecoveryRequestRequest& request,
      GetRecoveryRequestCallback callback) override;
  void GetAuthSessionStatus(
      const ::user_data_auth::GetAuthSessionStatusRequest& request,
      GetAuthSessionStatusCallback callback) override;
  void PrepareAuthFactor(
      const ::user_data_auth::PrepareAuthFactorRequest& request,
      PrepareAuthFactorCallback callback) override;
  void TerminateAuthFactor(
      const ::user_data_auth::TerminateAuthFactorRequest& request,
      TerminateAuthFactorCallback callback) override;

  // Sets the CryptohomeError value to return during next operation.
  void SetNextOperationError(Operation operation,
                             ::user_data_auth::CryptohomeErrorCode error);

  // Returns the `unlock_webauthn_secret` parameter passed in the last
  // CheckKeyEx call (either successful or not).
  bool get_last_unlock_webauthn_secret() {
    return last_unlock_webauthn_secret_;
  }

  // Getter for the AccountIdentifier() that was passed to the last
  // StartMigrateToDircrypto() call.
  const cryptohome::AccountIdentifier& get_id_for_disk_migrated_to_dircrypto()
      const {
    return last_migrate_to_dircrypto_request_.account_id();
  }
  // Whether the last StartMigrateToDircrypto() call indicates minimal
  // migration.
  bool minimal_migration() const {
    return last_migrate_to_dircrypto_request_.minimal_migration();
  }

  int get_prepare_guest_request_count() const {
    return prepare_guest_request_count_;
  }

  const ::user_data_auth::AddAuthFactorRequest&
  get_last_add_authfactor_request() const {
    return last_add_auth_factor_request_;
  }

  const ::user_data_auth::AuthenticateAuthFactorRequest&
  get_last_authenticate_auth_factor_request() const {
    return last_authenticate_auth_factor_request_;
  }

  // Calls DircryptoMigrationProgress() on Observer instances.
  void NotifyDircryptoMigrationProgress(
      ::user_data_auth::DircryptoMigrationStatus status,
      uint64_t current,
      uint64_t total);

  // Calls LowDiskSpace() on Observer instances.
  void NotifyLowDiskSpace(uint64_t disk_free_bytes);

  // Reads synchronously from disk, so must only be called in a scope that
  // allows blocking IO.
  void SetUserDataDir(base::FilePath path);

 private:
  enum class AuthResult {
    kAuthSuccess,
    kUserNotFound,
    kFactorNotFound,
    kAuthFailed,
  };

  // Helper that returns the protobuf reply.
  template <typename ReplyType>
  void ReturnProtobufMethodCallback(
      const ReplyType& reply,
      chromeos::DBusMethodCallback<ReplyType> callback);

  // This method is used to implement StartMigrateToDircrypto with simulated
  // progress updates.
  void OnDircryptoMigrationProgressUpdated();

  // Returns a path to home directory for account.
  absl::optional<base::FilePath> GetUserProfileDir(
      const cryptohome::AccountIdentifier& account_id) const;

  // The method takes serialized auth session id and returns an authenticated
  // auth session associated with the id. If the session is missing or not
  // authenticated, an error code is assigned to |*error| and |nullptr| is
  // returned.
  const AuthSessionData* GetAuthenticatedAuthSession(
      const std::string& auth_session_id,
      ::user_data_auth::CryptohomeErrorCode* error) const;

  void RunPendingWaitForServiceToBeAvailableCallbacks();

  // Checks the given credentials against the fake factors configured for the
  // given user. If `wildcard_allowed` is true and `factor_label` is empty,
  // every configured factor is attempted; `matched_factor_label` can be passed
  // in order to know the found factor's label.
  AuthResult AuthenticateViaAuthFactors(
      const cryptohome::AccountIdentifier& account_id,
      const std::string& factor_label,
      const std::string& secret,
      bool wildcard_allowed,
      std::string* matched_factor_label = nullptr) const;

  // Checks if there is a per-operation error defined, and uses it.
  ::user_data_auth::CryptohomeErrorCode TakeOperationError(Operation operation);

  int prepare_guest_request_count_ = 0;

  // The `unlock_webauthn_secret` parameter passed in the last CheckKeyEx call.
  bool last_unlock_webauthn_secret_;

  // The error that would be triggered once operation is called.
  base::flat_map<Operation, ::user_data_auth::CryptohomeErrorCode>
      operation_errors_;

  // The collection of users we know about.
  base::flat_map<cryptohome::AccountIdentifier, UserCryptohomeState> users_;

  // Timer for triggering the dircrypto migration progress signal.
  base::RepeatingTimer dircrypto_migration_progress_timer_;

  // The current dircrypto migration progress indicator, used when we trigger
  // the migration progress signal.
  uint64_t dircrypto_migration_progress_ = 0;

  // The StartMigrateToDircryptoRequest passed in for the last
  // StartMigrateToDircrypto() call.
  ::user_data_auth::StartMigrateToDircryptoRequest
      last_migrate_to_dircrypto_request_;

  // The AuthenticateAuthFactorRequest passed in for the last
  // AuthenticateAuthFactor() call.
  ::user_data_auth::AuthenticateAuthFactorRequest
      last_authenticate_auth_factor_request_;

  // The AddAuthFactorRequest passed in for the last AddAuthFactor() call.
  ::user_data_auth::AddAuthFactorRequest last_add_auth_factor_request_;

  // The auth sessions on file.
  base::flat_map<std::string, AuthSessionData> auth_sessions_;

  // Next available auth session id.
  int next_auth_session_id_ = 0;

  // The list of callbacks passed to WaitForServiceToBeAvailable when the
  // service wasn't available.
  std::vector<chromeos::WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  // Other stuff/miscellaneous:

  // Base directory of user directories.
  absl::optional<base::FilePath> user_data_dir_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;

  // List of fingerprint event observers.
  base::ObserverList<FingerprintAuthObserver> fingerprint_observers_;

  // Do we run the dircrypto migration, as in, emit signals, when
  // StartMigrateToDircrypto() is called?
  bool run_default_dircrypto_migration_ = true;

  // If low entropy credentials are supported for the key. This is the value
  // that GetSupportedKeyPolicies() returns.
  bool supports_low_entropy_credentials_ = false;

  // If true, authentication requests actually check the key.
  bool enable_auth_check_ = false;

  // If set, we tell callers that service is available.
  bool service_is_available_ = true;

  // If set, WaitForServiceToBeAvailable will run the callback, even if
  // service is not available (instead of adding the callback to pending
  // callback list).
  bool service_reported_not_available_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
