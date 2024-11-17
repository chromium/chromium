// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_

#include <optional>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/cryptohome/error_types.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/account_identifier_operators.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

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
    kUpdateAuthFactor,
    kUpdateAuthFactorMetadata,
    kReplaceAuthFactor,
    kListAuthFactors,
    kStartMigrateToDircrypto,
    kRemove,
    kGetRecoverableKeyStores,
  };

  // The method by which a user's home directory can be encrypted.
  enum class HomeEncryptionMethod {
    kDirCrypto,
    kEcryptfs,
    kDmCrypt,
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

    // Sets whether ARC disk quota is supported or not.
    void set_arc_quota_supported(bool supported) {
      FakeUserDataAuthClient::Get()->arc_quota_supported_ = supported;
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
    std::optional<base::FilePath> GetUserProfileDir(
        const cryptohome::AccountIdentifier& account_id) const;

    // Creates user directories once UserDataDir is available.
    void CreatePostponedDirectories();

    // Adds the given key as a fake auth factor to the user (the user must
    // already exist).
    void AddAuthFactor(const cryptohome::AccountIdentifier& account_id,
                       const user_data_auth::AuthFactor& factor,
                       const user_data_auth::AuthInput& input);

    void AddRecoveryFactor(const cryptohome::AccountIdentifier& account_id);
    bool HasRecoveryFactor(const cryptohome::AccountIdentifier& account_id);

    bool HasPinFactor(const cryptohome::AccountIdentifier& account_id);

    // Returns {authsession_id, broadcast_id} pair.
    std::pair<std::string, std::string> AddSession(
        const cryptohome::AccountIdentifier& account_id,
        bool authenticated);

    // Checks that there is one active auth session and returns whether session
    // is ephemeral.
    bool IsCurrentSessionEphemeral();

    void DestroySessions();

    void SendLegacyFPAuthSignal(user_data_auth::FingerprintScanResult result);

    // Sets the CryptohomeError value to return during next operation.
    void SetNextOperationError(Operation operation,
                               ::cryptohome::ErrorWrapper error);

    bool IsAuthenticated(const cryptohome::AccountIdentifier& account_id);

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
    std::string broadcast_id;
    // Whether the is_ephemeral_user flag was set on creation.
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
    base::Time lifetime;
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
  void AddPrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) override;
  void RemovePrepareAuthFactorProgressObserver(
      PrepareAuthFactorProgressObserver* observer) override;
  void AddAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) override;
  void RemoveAuthFactorStatusUpdateObserver(
      AuthFactorStatusUpdateObserver* observer) override;
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                 IsMountedCallback callback) override;
  void GetVaultProperties(
      const ::user_data_auth::GetVaultPropertiesRequest& request,
      GetVaultPropertiesCallback callback) override;
  void Unmount(const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback) override;
  void Remove(const ::user_data_auth::RemoveRequest& request,
              RemoveCallback callback) override;
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
  void UpdateAuthFactorMetadata(
      const ::user_data_auth::UpdateAuthFactorMetadataRequest& request,
      UpdateAuthFactorMetadataCallback callback) override;
  void ReplaceAuthFactor(
      const ::user_data_auth::ReplaceAuthFactorRequest& request,
      ReplaceAuthFactorCallback callback) override;
  void RemoveAuthFactor(
      const ::user_data_auth::RemoveAuthFactorRequest& request,
      RemoveAuthFactorCallback callback) override;
  void ListAuthFactors(const ::user_data_auth::ListAuthFactorsRequest& request,
                       ListAuthFactorsCallback callback) override;
  void GetAuthFactorExtendedInfo(
      const ::user_data_auth::GetAuthFactorExtendedInfoRequest& request,
      GetAuthFactorExtendedInfoCallback callback) override;
  void GetAuthSessionStatus(
      const ::user_data_auth::GetAuthSessionStatusRequest& request,
      GetAuthSessionStatusCallback callback) override;
  void PrepareAuthFactor(
      const ::user_data_auth::PrepareAuthFactorRequest& request,
      PrepareAuthFactorCallback callback) override;
  void TerminateAuthFactor(
      const ::user_data_auth::TerminateAuthFactorRequest& request,
      TerminateAuthFactorCallback callback) override;
  void GetArcDiskFeatures(
      const ::user_data_auth::GetArcDiskFeaturesRequest& request,
      GetArcDiskFeaturesCallback callback) override;
  void GetRecoverableKeyStores(
      const ::user_data_auth::GetRecoverableKeyStoresRequest& request,
      GetRecoverableKeyStoresCallback) override;
  void SetUserDataStorageWriteEnabled(
      const ::user_data_auth::SetUserDataStorageWriteEnabledRequest& request,
      SetUserDataStorageWriteEnabledCallback callback) override;

  int get_prepare_guest_request_count() const {
    return prepare_guest_request_count_;
  }

  // Per-operation API:
  template <Operation>
  struct ProtobufTypes;

  // Template magic to have mapping from operation to
  // associated types for protobufs.
#define FUDAC_OPERATION_TYPES(OPERATION, REQUEST)  \
  template <>                                      \
  struct ProtobufTypes<Operation::OPERATION> {     \
    using RequestType = ::user_data_auth::REQUEST; \
  }

  FUDAC_OPERATION_TYPES(kStartAuthSession, StartAuthSessionRequest);
  FUDAC_OPERATION_TYPES(kAuthenticateAuthFactor, AuthenticateAuthFactorRequest);
  FUDAC_OPERATION_TYPES(kPrepareGuestVault, PrepareGuestVaultRequest);
  FUDAC_OPERATION_TYPES(kPrepareEphemeralVault, PrepareEphemeralVaultRequest);
  FUDAC_OPERATION_TYPES(kCreatePersistentUser, CreatePersistentUserRequest);
  FUDAC_OPERATION_TYPES(kPreparePersistentVault, PreparePersistentVaultRequest);
  FUDAC_OPERATION_TYPES(kPrepareVaultForMigration,
                        PrepareVaultForMigrationRequest);
  FUDAC_OPERATION_TYPES(kAddAuthFactor, AddAuthFactorRequest);
  FUDAC_OPERATION_TYPES(kUpdateAuthFactor, UpdateAuthFactorRequest);
  FUDAC_OPERATION_TYPES(kUpdateAuthFactorMetadata,
                        UpdateAuthFactorMetadataRequest);
  FUDAC_OPERATION_TYPES(kReplaceAuthFactor, ReplaceAuthFactorRequest);
  FUDAC_OPERATION_TYPES(kListAuthFactors, ListAuthFactorsRequest);
  FUDAC_OPERATION_TYPES(kStartMigrateToDircrypto,
                        StartMigrateToDircryptoRequest);
  FUDAC_OPERATION_TYPES(kRemove, RemoveRequest);
  FUDAC_OPERATION_TYPES(kGetRecoverableKeyStores,
                        GetRecoverableKeyStoresRequest);

#undef FUDAC_OPERATION_TYPES

  // Sets the CryptohomeError value to return during next operation.
  void SetNextOperationError(Operation operation,
                             ::cryptohome::ErrorWrapper error);

  // Checks if operation was called.
  template <Operation Op>
  bool WasCalled() {
    const auto op_request = operation_requests_.find(Op);
    return op_request != std::end(operation_requests_);
  }

  // Provides request protobuf passed to last call of operation.
  // Would crash if operation was not called before, use `WasCalled()` to
  // check.
  template <Operation Op>
  const typename ProtobufTypes<Op>::RequestType& GetLastRequest() {
    const auto op_request = operation_requests_.find(Op);
    CHECK(op_request != std::end(operation_requests_));
    return *static_cast<const typename ProtobufTypes<Op>::RequestType*>(
        op_request->second.get());
  }

  // See also `RememberRequest` in private section.

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

  // Adds a default Gaia password factor if none other auth factors exist
  // when AuthSession starts.
  void set_add_default_password_factor(bool add_default_password_factor) {
    add_default_password_factor_ = add_default_password_factor;
  }

 private:
  enum class AuthResult {
    kAuthSuccess,
    kUserNotFound,
    kFactorNotFound,
    kAuthFailed,
  };

  // Utility method to remember request passed to operation in a
  // type-safe way.
  template <Operation Op>
  void RememberRequest(const typename ProtobufTypes<Op>::RequestType& request) {
    operation_requests_[Op] =
        std::make_unique<typename ProtobufTypes<Op>::RequestType>(request);
  }

  // Helper that returns the protobuf reply.
  template <typename ReplyType>
  void ReturnProtobufMethodCallback(
      const ReplyType& reply,
      chromeos::DBusMethodCallback<ReplyType> callback);

  // This method is used to implement StartMigrateToDircrypto with simulated
  // progress updates.
  void OnDircryptoMigrationProgressUpdated();

  // Returns a path to home directory for account.
  std::optional<base::FilePath> GetUserProfileDir(
      const cryptohome::AccountIdentifier& account_id) const;

  // The method takes serialized auth session id and returns an authenticated
  // auth session associated with the id. If the session is missing or not
  // authenticated, an error code is assigned to |*error| and |nullptr| is
  // returned.
  const AuthSessionData* GetAuthenticatedAuthSession(
      const std::string& auth_session_id,
      ::cryptohome::ErrorWrapper* error) const;

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
  ::cryptohome::ErrorWrapper TakeOperationError(Operation operation);

  int prepare_guest_request_count_ = 0;

  // The error that would be triggered once operation is called.
  base::flat_map<Operation, ::cryptohome::ErrorWrapper> operation_errors_;

  // Remembered requests.
  base::flat_map<Operation, std::unique_ptr<::google::protobuf::MessageLite>>
      operation_requests_;

  // The collection of users we know about.
  base::flat_map<cryptohome::AccountIdentifier, UserCryptohomeState> users_;

  // Timer for triggering the dircrypto migration progress signal.
  base::RepeatingTimer dircrypto_migration_progress_timer_;

  // The current dircrypto migration progress indicator, used when we trigger
  // the migration progress signal.
  uint64_t dircrypto_migration_progress_ = 0;

  // The auth sessions on file.
  base::flat_map<std::string, AuthSessionData> auth_sessions_;

  // Next available auth session id.
  int next_auth_session_id_ = 0;

  // The list of callbacks passed to WaitForServiceToBeAvailable when the
  // service wasn't available.
  std::vector<chromeos::WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  // The list of usernames of users with mounted user dirs.
  std::set<std::string> mounted_user_dirs_;

  // Other stuff/miscellaneous:

  // Base directory of user directories.
  std::optional<base::FilePath> user_data_dir_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;

  // List of legacy fingerprint event observers.
  base::ObserverList<FingerprintAuthObserver> fingerprint_observers_;

  // List of PrepareAuthFactorProgress event observers.
  base::ObserverList<PrepareAuthFactorProgressObserver> progress_observers_;

  // List of observers for dbus signal AuthFactorStatusUpdate.
  base::ObserverList<AuthFactorStatusUpdateObserver>
      auth_factor_status_observer_list_;

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

  // Whether ARC disk quota is supported or not.
  bool arc_quota_supported_ = true;

  // If set, WaitForServiceToBeAvailable will run the callback, even if
  // service is not available (instead of adding the callback to pending
  // callback list).
  bool service_reported_not_available_ = false;

  // If set, adds a default Gaia password factor when AuthSession starts.
  bool add_default_password_factor_ = false;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
