// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRYPTOHOME_FAKE_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_CRYPTOHOME_FAKE_CRYPTOHOME_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/key.pb.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(CRYPTOHOME_CLIENT) FakeCryptohomeClient
    : public CryptohomeClient {
 public:
  // FakeCryptohomeClient can be embedded in unit tests, but the
  // InitializeFake/Shutdown pattern should be preferred. Constructing the
  // instance will set the global instance for the fake and for the base class,
  // so the static Get() accessor can be used with that pattern.
  FakeCryptohomeClient();
  ~FakeCryptohomeClient() override;

  // Checks that a FakeCryptohome instance was initialized and returns it.
  static FakeCryptohomeClient* Get();

  // Expose stub password for tests.
  static const char kStubTpmPassword[];

  // CryptohomeClient overrides
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void IsMounted(DBusMethodCallback<bool> callback) override;
  void UnmountEx(const cryptohome::UnmountRequest& request,
                 DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void MigrateKeyEx(
      const cryptohome::AccountIdentifier& account,
      const cryptohome::AuthorizationRequest& auth_request,
      const cryptohome::MigrateKeyRequest& migrate_request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void RemoveEx(const cryptohome::AccountIdentifier& account,
                DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void RenameCryptohome(
      const cryptohome::AccountIdentifier& cryptohome_id_from,
      const cryptohome::AccountIdentifier& cryptohome_id_to,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetAccountDiskUsage(
      const cryptohome::AccountIdentifier& account_id,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetSystemSalt(
      DBusMethodCallback<std::vector<uint8_t>> callback) override;
  void GetSanitizedUsername(const cryptohome::AccountIdentifier& cryptohome_id,
                            DBusMethodCallback<std::string> callback) override;
  std::string BlockingGetSanitizedUsername(
      const cryptohome::AccountIdentifier& cryptohome_id) override;
  void MountGuestEx(
      const cryptohome::MountGuestRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetRsuDeviceId(
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void TpmIsReady(DBusMethodCallback<bool> callback) override;
  void TpmIsEnabled(DBusMethodCallback<bool> callback) override;
  bool CallTpmIsEnabledAndBlock(bool* enabled) override;
  void TpmGetPassword(DBusMethodCallback<std::string> callback) override;
  void TpmIsOwned(DBusMethodCallback<bool> callback) override;
  bool CallTpmIsOwnedAndBlock(bool* owned) override;
  void TpmIsBeingOwned(DBusMethodCallback<bool> callback) override;
  bool CallTpmIsBeingOwnedAndBlock(bool* owning) override;
  void TpmCanAttemptOwnership(VoidDBusMethodCallback callback) override;
  void TpmClearStoredPassword(VoidDBusMethodCallback callback) override;
  bool CallTpmClearStoredPasswordAndBlock() override;
  void Pkcs11IsTpmTokenReady(DBusMethodCallback<bool> callback) override;
  void Pkcs11GetTpmTokenInfo(
      DBusMethodCallback<TpmTokenInfo> callback) override;
  void Pkcs11GetTpmTokenInfoForUser(
      const cryptohome::AccountIdentifier& cryptohome_id,
      DBusMethodCallback<TpmTokenInfo> callback) override;
  bool InstallAttributesGet(const std::string& name,
                            std::vector<uint8_t>* value,
                            bool* successful) override;
  bool InstallAttributesSet(const std::string& name,
                            const std::vector<uint8_t>& value,
                            bool* successful) override;
  bool InstallAttributesFinalize(bool* successful) override;
  void InstallAttributesIsReady(DBusMethodCallback<bool> callback) override;
  bool InstallAttributesIsInvalid(bool* is_invalid) override;
  bool InstallAttributesIsFirstInstall(bool* is_first_install) override;
  void TpmAttestationIsPrepared(DBusMethodCallback<bool> callback) override;
  void TpmAttestationGetEnrollmentId(
      bool ignore_cache,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationIsEnrolled(DBusMethodCallback<bool> callback) override;
  void AsyncTpmAttestationCreateEnrollRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      AsyncMethodCallback callback) override;
  void AsyncTpmAttestationEnroll(chromeos::attestation::PrivacyCAType pca_type,
                                 const std::string& pca_response,
                                 AsyncMethodCallback callback) override;
  void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& request_origin,
      AsyncMethodCallback callback) override;
  void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      AsyncMethodCallback callback) override;
  void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<bool> callback) override;
  void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationRegisterKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      AsyncMethodCallback callback) override;
  void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const std::string& key_name_for_spkac,
      AsyncMethodCallback callback) override;
  void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      const std::string& challenge,
      AsyncMethodCallback callback) override;
  void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) override;
  void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      const std::string& payload,
      DBusMethodCallback<bool> callback) override;
  void TpmAttestationDeleteKeys(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_prefix,
      DBusMethodCallback<bool> callback) override;
  void TpmGetVersion(DBusMethodCallback<TpmVersionInfo> callback) override;
  void GetKeyDataEx(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::GetKeyDataRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void CheckKeyEx(const cryptohome::AccountIdentifier& cryptohome_id,
                  const cryptohome::AuthorizationRequest& auth,
                  const cryptohome::CheckKeyRequest& request,
                  DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void MountEx(const cryptohome::AccountIdentifier& cryptohome_id,
               const cryptohome::AuthorizationRequest& auth,
               const cryptohome::MountRequest& request,
               DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void LockToSingleUserMountUntilReboot(
      const cryptohome::LockToSingleUserMountUntilRebootRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void AddKeyEx(const cryptohome::AccountIdentifier& cryptohome_id,
                const cryptohome::AuthorizationRequest& auth,
                const cryptohome::AddKeyRequest& request,
                DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void AddDataRestoreKey(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const cryptohome::AuthorizationRequest& auth,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void UpdateKeyEx(const cryptohome::AccountIdentifier& cryptohome_id,
                   const cryptohome::AuthorizationRequest& auth,
                   const cryptohome::UpdateKeyRequest& request,
                   DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void RemoveKeyEx(const cryptohome::AccountIdentifier& cryptohome_id,
                   const cryptohome::AuthorizationRequest& auth,
                   const cryptohome::RemoveKeyRequest& request,
                   DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void MassRemoveKeys(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::MassRemoveKeysRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void SetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void GetTpmStatus(
      const cryptohome::GetTpmStatusRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void MigrateToDircrypto(const cryptohome::AccountIdentifier& cryptohome_id,
                          const cryptohome::MigrateToDircryptoRequest& request,
                          VoidDBusMethodCallback callback) override;
  void RemoveFirmwareManagementParametersFromTpm(
      const cryptohome::RemoveFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void SetFirmwareManagementParametersInTpm(
      const cryptohome::SetFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void NeedsDircryptoMigration(
      const cryptohome::AccountIdentifier& cryptohome_id,
      DBusMethodCallback<bool> callback) override;
  void GetSupportedKeyPolicies(
      const cryptohome::GetSupportedKeyPoliciesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) override;
  void IsQuotaSupported(DBusMethodCallback<bool> callback) override;
  void GetCurrentSpaceForUid(uid_t android_uid,
                             DBusMethodCallback<int64_t> callback) override;
  void GetCurrentSpaceForGid(gid_t android_gid,
                             DBusMethodCallback<int64_t> callback) override;

  /////////// Test helpers ////////////

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  // Runs pending availability callbacks reporting that the service is
  // unavailable. Expects service not to be available when called.
  void ReportServiceIsNotAvailable();

  // Changes the behavior of TpmIsReady().
  void set_tpm_is_ready(bool value) { tpm_is_ready_ = value; }

  // Changes the behavior of TpmIsEnabled().
  void set_tpm_is_enabled(bool value) { tpm_is_enabled_ = value; }

  // Sets whether the MountEx() call should fail when the |create| field is not
  // provided (the error code will be CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND).
  // This allows to simulate the behavior during the new user profile creation.
  void set_mount_create_required(bool mount_create_required) {
    mount_create_required_ = mount_create_required;
  }

  // Sets the unmount result of Unmount() call.
  void set_unmount_result(bool result) { unmount_result_ = result; }

  // Sets the system salt which will be returned from GetSystemSalt(). By
  // default, GetSystemSalt() returns the value generated by
  // GetStubSystemSalt().
  void set_system_salt(const std::vector<uint8_t>& system_salt) {
    system_salt_ = system_salt;
  }

  // Returns the stub system salt as raw bytes. (not as a string encoded in the
  // format used by SystemSaltGetter::ConvertRawSaltToHexString()).
  static std::vector<uint8_t> GetStubSystemSalt();

  // Sets the needs dircrypto migration value.
  void set_needs_dircrypto_migration(bool needs_migration) {
    needs_dircrypto_migration_ = needs_migration;
  }

  // Sets whether dircrypto migration update should be run automatically.
  // If set to false, the client will not send any dircrypto migration progress
  // updates on its own - a test that sets this will have to call
  // NotifyDircryptoMigrationProgress() for the progress to update.
  void set_run_default_dircrypto_migration(bool value) {
    run_default_dircrypto_migration_ = value;
  }

  // Sets the CryptohomeError value to return.
  void set_cryptohome_error(cryptohome::CryptohomeErrorCode error) {
    cryptohome_error_ = error;
  }

  void set_tpm_attestation_enrollment_id(bool ignore_cache,
                                         const std::string& eid) {
    if (ignore_cache) {
      tpm_attestation_enrollment_id_ignore_cache_ = eid;
    } else {
      tpm_attestation_enrollment_id_ = eid;
    }
  }

  void set_tpm_attestation_is_enrolled(bool enrolled) {
    tpm_attestation_is_enrolled_ = enrolled;
  }

  void set_tpm_attestation_is_prepared(bool prepared) {
    tpm_attestation_is_prepared_ = prepared;
  }

  void set_tpm_attestation_does_key_exist_should_succeed(bool should_succeed) {
    tpm_attestation_does_key_exist_should_succeed_ = should_succeed;
  }

  void set_supports_low_entropy_credentials(bool supports) {
    supports_low_entropy_credentials_ = supports;
  }

  void set_enable_auth_check(bool enable_auth_check) {
    enable_auth_check_ = enable_auth_check;
  }

  void set_rsu_device_id(const std::string& rsu_device_id) {
    rsu_device_id_ = rsu_device_id;
  }

  void SetTpmAttestationUserCertificate(
      const cryptohome::AccountIdentifier& cryptohome_id,
      const std::string& key_name,
      const std::string& certificate);

  void SetTpmAttestationDeviceCertificate(const std::string& key_name,
                                          const std::string& certificate);

  base::Optional<std::string> GetTpmAttestationDeviceKeyPayload(
      const std::string& key_name) const;

  void SetTpmAttestationDeviceKeyPayload(const std::string& key_name,
                                         const std::string& payload);

  // Calls TpmInitStatusUpdated() on Observer instances.
  void NotifyTpmInitStatusUpdated(bool ready,
                                  bool owned,
                                  bool was_owned_this_boot);

  // Calls DircryptoMigrationProgress() on Observer instances.
  void NotifyDircryptoMigrationProgress(
      cryptohome::DircryptoMigrationStatus status,
      uint64_t current,
      uint64_t total);

  // Notifies LowDiskSpace() to Observer instances.
  void NotifyLowDiskSpace(uint64_t disk_free_bytes);

  // MountEx getters.
  const cryptohome::MountRequest& get_last_mount_request() const {
    return last_mount_request_;
  }
  bool to_migrate_from_ecryptfs() const {
    return last_mount_request_.to_migrate_from_ecryptfs();
  }
  bool hidden_mount() const { return last_mount_request_.hidden_mount(); }
  bool public_mount() const { return last_mount_request_.public_mount(); }
  const cryptohome::AuthorizationRequest& get_last_mount_authentication()
      const {
    return last_mount_auth_request_;
  }
  const std::string& get_secret_for_last_mount_authentication() const {
    return last_mount_auth_request_.key().secret();
  }

  // MigrateToDircrypto getters.
  const cryptohome::AccountIdentifier& get_id_for_disk_migrated_to_dircrypto()
      const {
    return id_for_disk_migrated_to_dircrypto_;
  }
  bool minimal_migration() const {
    return last_migrate_to_dircrypto_request_.minimal_migration();
  }

  int remove_firmware_management_parameters_from_tpm_call_count() const {
    return remove_firmware_management_parameters_from_tpm_call_count_;
  }

  bool is_device_locked_to_single_user() const {
    return is_device_locked_to_single_user_;
  }

 private:
  void ReturnProtobufMethodCallback(
      const cryptohome::BaseReply& reply,
      DBusMethodCallback<cryptohome::BaseReply> callback);

  // Posts tasks which return fake results to the UI thread.
  void ReturnAsyncMethodResult(AsyncMethodCallback callback);

  // Posts tasks which return fake data to the UI thread.
  void ReturnAsyncMethodData(AsyncMethodCallback callback,
                             const std::string& data);

  // This method is used to implement ReturnAsyncMethodResult without data.
  void ReturnAsyncMethodResultInternal(AsyncMethodCallback callback);

  // This method is used to implement ReturnAsyncMethodResult with data.
  void ReturnAsyncMethodDataInternal(AsyncMethodCallback callback,
                                     const std::string& data);

  // This method is used to implement MigrateToDircrypto with simulated progress
  // updates.
  void OnDircryptoMigrationProgressUpdated();

  // Notifies AsyncCallStatus() to Observer instances.
  void NotifyAsyncCallStatus(int async_id, bool return_status, int return_code);

  // Notifies AsyncCallStatusWithData() to Observer instances.
  void NotifyAsyncCallStatusWithData(int async_id,
                                     bool return_status,
                                     const std::string& data);

  // Loads install attributes from the stub file.
  bool LoadInstallAttributes();

  // Finds a key matching the given label. Wildcard labels are supported.
  std::map<std::string, cryptohome::Key>::const_iterator FindKey(
      const std::map<std::string, cryptohome::Key>& keys,
      const std::string& label);

  bool service_is_available_ = true;
  // If set, WaitForServiceToBeAvailable will run the callback, even if service
  // is not available (instead of adding the callback to pending callback list).
  bool service_reported_not_available_ = false;
  base::ObserverList<Observer>::Unchecked observer_list_;

  int remove_firmware_management_parameters_from_tpm_call_count_ = 0;

  int async_call_id_ = 1;
  bool mount_create_required_ = false;
  bool unmount_result_ = true;
  std::vector<uint8_t> system_salt_{GetStubSystemSalt()};

  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;

  // A stub store for InstallAttributes, mapping an attribute name to the
  // associated data blob. Used to implement InstallAttributesSet and -Get.
  std::map<std::string, std::vector<uint8_t>> install_attrs_;
  bool locked_;

  std::map<cryptohome::AccountIdentifier,
           std::map<std::string, cryptohome::Key>>
      key_data_map_;

  // User attestation certificate mapped by cryptohome_id and key_name.
  std::map<std::pair<cryptohome::AccountIdentifier, std::string>, std::string>
      user_certificate_map_;

  // Device attestation certificate mapped by key_name.
  std::map<std::string, std::string> device_certificate_map_;

  // Device key payload data mapped by key_name.
  std::map<std::string, std::string> device_key_payload_map_;

  base::RepeatingTimer dircrypto_migration_progress_timer_;
  uint64_t dircrypto_migration_progress_ = 0;

  bool needs_dircrypto_migration_ = false;
  bool run_default_dircrypto_migration_ = true;
  std::string tpm_attestation_enrollment_id_ignore_cache_ =
      "6fcc0ebddec3db95cdcf82476d594f4d60db934c5b47fa6085c707b2a93e205b";
  std::string tpm_attestation_enrollment_id_ =
      "6fcc0ebddec3db95cdcf82476d594f4d60db934c5b47fa6085c707b2a93e205b";
  bool tpm_attestation_is_enrolled_ = true;
  bool tpm_attestation_is_prepared_ = true;
  bool tpm_attestation_does_key_exist_should_succeed_ = true;
  bool supports_low_entropy_credentials_ = false;
  // Controls if CheckKeyEx actually checks the key.
  bool enable_auth_check_ = false;
  bool tpm_is_ready_ = true;
  bool tpm_is_enabled_ = true;

  // Reply to GetRsuDeviceId().
  std::string rsu_device_id_;

  // MountEx fields.
  cryptohome::CryptohomeErrorCode cryptohome_error_ =
      cryptohome::CRYPTOHOME_ERROR_NOT_SET;
  cryptohome::MountRequest last_mount_request_;
  cryptohome::AuthorizationRequest last_mount_auth_request_;

  // MigrateToDircrypto fields.
  cryptohome::AccountIdentifier id_for_disk_migrated_to_dircrypto_;
  cryptohome::MigrateToDircryptoRequest last_migrate_to_dircrypto_request_;

  // Used by LockToSingleUserMountUntilReboot.
  bool is_device_locked_to_single_user_ = false;

  base::WeakPtrFactory<FakeCryptohomeClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeCryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRYPTOHOME_FAKE_CRYPTOHOME_CLIENT_H_
