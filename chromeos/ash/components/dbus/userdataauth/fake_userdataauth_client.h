// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/dbus/cryptohome/account_identifier_operators.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) FakeUserDataAuthClient
    : public UserDataAuthClient {
 public:
  // The method by which a user's home directory can be encrypted.
  enum class HomeEncryptionMethod {
    kDirCrypto,
    kEcryptfs,
  };

  class COMPONENT_EXPORT(USERDATAAUTH_CLIENT) TestApi {
   public:
    ~TestApi() = default;

    // Not copyable or movable.
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;
    TestApi(TestApi&&) = delete;
    TestApi& operator=(TestApi&&) = delete;

    static TestApi* Get();

    // Sets whether dircrypto migration update should be run automatically.
    // If set to false, the client will not send any dircrypto migration
    // progress updates on its own - a test that sets this will have to call
    // NotifyDircryptoMigrationProgress() for the progress to update.
    void set_run_default_dircrypto_migration(bool value) {
      run_default_dircrypto_migration_ = value;
    }

    // If set, next call to GetSupportedKeyPolicies() will tell caller that low
    // entropy credentials are supported.
    void set_supports_low_entropy_credentials(bool supports) {
      supports_low_entropy_credentials_ = supports;
    }

    // If enable_auth_check is true, then CheckKey will actually check the
    // authorization.
    void set_enable_auth_check(bool enable_auth_check) {
      enable_auth_check_ = enable_auth_check;
    }

    // Sets whether the Mount() call should fail when the |create| field is not
    // provided (the error code will be CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND).
    // This allows to simulate the behavior during the new user profile
    // creation.
    void set_mount_create_required(bool mount_create_required) {
      mount_create_required_ = mount_create_required;
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

    // Marks a PIN key as locked or unlocked. The key is identified by the
    // |account_id| of the user it belongs to and its |label|. The key must
    // exist prior to this call, and it must be a PIN key.
    void SetPinLocked(const cryptohome::AccountIdentifier& account_id,
                      const std::string& label,
                      bool locked);

    // Marks a user as existing and creates the user's home directory. No auth
    // factors are added.
    void AddExistingUser(cryptohome::AccountIdentifier account_id);

   private:
    friend class FakeUserDataAuthClient;

    explicit TestApi(base::raw_ptr<FakeUserDataAuthClient> client);

    // The singleton instance
    static base::raw_ptr<FakeUserDataAuthClient::TestApi> instance_;

    // Do we run the dircrypto migration, as in, emit signals, when
    // StartMigrateToDircrypto() is called?
    bool run_default_dircrypto_migration_ = true;

    // If low entropy credentials are supported for the key. This is the value
    // that GetSupportedKeyPolicies() returns.
    bool supports_low_entropy_credentials_ = false;

    // Controls if CheckKeyEx actually checks the key.
    bool enable_auth_check_ = false;

    // If true, fails if |create| field is not provided
    bool mount_create_required_ = false;

    // If set, we tell callers that service is available.
    bool service_is_available_ = true;

    // If set, WaitForServiceToBeAvailable will run the callback, even if
    // service is not available (instead of adding the callback to pending
    // callback list).
    bool service_reported_not_available_ = false;

    base::raw_ptr<FakeUserDataAuthClient> client_;
  };

  // Represents the ongoing AuthSessions.
  struct AuthSessionData {
    // AuthSession id.
    std::string id;
    // Account associated with the session.
    cryptohome::AccountIdentifier account;
    // True if session is authenticated.
    bool authenticated = false;
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
  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(const ::user_data_auth::IsMountedRequest& request,
                 IsMountedCallback callback) override;
  void Unmount(const ::user_data_auth::UnmountRequest& request,
               UnmountCallback callback) override;
  void Mount(const ::user_data_auth::MountRequest& request,
             MountCallback callback) override;
  void Remove(const ::user_data_auth::RemoveRequest& request,
              RemoveCallback callback) override;
  void GetKeyData(const ::user_data_auth::GetKeyDataRequest& request,
                  GetKeyDataCallback callback) override;
  void CheckKey(const ::user_data_auth::CheckKeyRequest& request,
                CheckKeyCallback callback) override;
  void AddKey(const ::user_data_auth::AddKeyRequest& request,
              AddKeyCallback callback) override;
  void RemoveKey(const ::user_data_auth::RemoveKeyRequest& request,
                 RemoveKeyCallback callback) override;
  void MassRemoveKeys(const ::user_data_auth::MassRemoveKeysRequest& request,
                      MassRemoveKeysCallback callback) override;
  void MigrateKey(const ::user_data_auth::MigrateKeyRequest& request,
                  MigrateKeyCallback callback) override;
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
  void AuthenticateAuthSession(
      const ::user_data_auth::AuthenticateAuthSessionRequest& request,
      AuthenticateAuthSessionCallback callback) override;
  void AddCredentials(const ::user_data_auth::AddCredentialsRequest& request,
                      AddCredentialsCallback callback) override;
  void UpdateCredential(
      const ::user_data_auth::UpdateCredentialRequest& request,
      UpdateCredentialCallback callback) override;
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
  void GetAuthSessionStatus(
      const ::user_data_auth::GetAuthSessionStatusRequest& request,
      GetAuthSessionStatusCallback callback) override;

  // Sets the CryptohomeError value to return.
  void set_cryptohome_error(::user_data_auth::CryptohomeErrorCode error) {
    cryptohome_error_ = error;
  }
  // Get the MountRequest to last Mount().
  int get_mount_request_count() const { return mount_request_count_; }
  const ::user_data_auth::MountRequest& get_last_mount_request() const {
    return last_mount_request_;
  }
  // If the last call to Mount() have to_migrate_from_ecryptfs set.
  bool to_migrate_from_ecryptfs() const {
    return last_mount_request_.to_migrate_from_ecryptfs();
  }
  // If the last call to Mount() have public_mount set.
  bool public_mount() const { return last_mount_request_.public_mount(); }
  // Return the authorization request passed to the last Mount().
  const cryptohome::AuthorizationRequest& get_last_mount_authentication()
      const {
    return last_mount_request_.authorization();
  }
  // Return the secret passed to last Mount().
  const std::string& get_secret_for_last_mount_authentication() const {
    return last_mount_request_.authorization().key().secret();
  }

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
  const ::cryptohome::AuthorizationRequest&
  get_last_authenticate_auth_session_authorization() const {
    return last_authenticate_auth_session_request_.authorization();
  }

  const ::cryptohome::AuthorizationRequest& get_last_add_credentials_request()
      const {
    return last_add_credentials_request_.authorization();
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
  struct UserCryptohomeState;

  // Helper that returns the protobuf reply.
  template <typename ReplyType>
  void ReturnProtobufMethodCallback(const ReplyType& reply,
                                    DBusMethodCallback<ReplyType> callback);

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

  ::user_data_auth::CryptohomeErrorCode cryptohome_error_ =
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET;
  int prepare_guest_request_count_ = 0;
  int mount_request_count_ = 0;
  ::user_data_auth::MountRequest last_mount_request_;

  // The `unlock_webauthn_secret` parameter passed in the last CheckKeyEx call.
  bool last_unlock_webauthn_secret_;

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

  // The AuthenticateAuthSessionRequest passed in for the last
  // AuthenticateAuthSession() call.
  ::user_data_auth::AuthenticateAuthSessionRequest
      last_authenticate_auth_session_request_;

  // The AddCredentialsRequest passed in for the last AddCredentials() call.
  ::user_data_auth::AddCredentialsRequest last_add_credentials_request_;

  // The AuthenticateAuthFactorRequest passed in for the last
  // AuthenticateAuthFactor() call.
  ::user_data_auth::AuthenticateAuthFactorRequest
      last_authenticate_auth_factor_request_;

  // The auth sessions on file.
  base::flat_map<std::string, AuthSessionData> auth_sessions_;

  // Next available auth session id.
  int next_auth_session_id_ = 0;

  // The list of callbacks passed to WaitForServiceToBeAvailable when the
  // service wasn't available.
  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  // Other stuff/miscellaneous:

  // Base directory of user directories.
  absl::optional<base::FilePath> user_data_dir_;

  // List of observers.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the migration is finished.
namespace chromeos {
using ::ash::FakeUserDataAuthClient;
}

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_USERDATAAUTH_FAKE_USERDATAAUTH_CLIENT_H_
