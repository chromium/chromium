// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CRYPTOHOME_CRYPTOHOME_CLIENT_H_
#define CHROMEOS_DBUS_CRYPTOHOME_CRYPTOHOME_CLIENT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace cryptohome {

class AccountIdentifier;
class AddKeyRequest;
class AuthorizationRequest;
class BaseReply;
class CheckHealthRequest;
class CheckKeyRequest;
class EndFingerprintAuthSessionRequest;
class FlushAndSignBootAttributesRequest;
class GetBootAttributeRequest;
class GetKeyDataRequest;
class GetLoginStatusRequest;
class GetSupportedKeyPoliciesRequest;
class LockToSingleUserMountUntilRebootRequest;
class MassRemoveKeysRequest;
class MigrateKeyRequest;
class MigrateToDircryptoRequest;
class MountGuestRequest;
class MountRequest;
class RemoveFirmwareManagementParametersRequest;
class RemoveKeyRequest;
class SetBootAttributeRequest;
class SetFirmwareManagementParametersRequest;
class StartFingerprintAuthSessionRequest;
class UnmountRequest;

}  // namespace cryptohome

namespace dbus {
class Bus;
}

namespace chromeos {

// CryptohomeClient is used to communicate with the Cryptohome service.
// All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(CRYPTOHOME_CLIENT) CryptohomeClient {
 public:
  class Observer {
   public:
    // Called when AsyncCallStatus signal is received, when results for
    // AsyncXXX methods are returned. Cryptohome service will process the
    // calls in a first-in-first-out manner when they are made in parallel.
    virtual void AsyncCallStatus(int async_id,
                                 bool return_status,
                                 int return_code) {}

    // Called when AsyncCallStatusWithData signal is received,
    // similar to AsyncCallStatus, but with |data|.
    virtual void AsyncCallStatusWithData(int async_id,
                                         bool return_status,
                                         const std::string& data) {}

    // Called when LowDiskSpace signal is received, when the cryptohome
    // partition is running out of disk space.
    virtual void LowDiskSpace(uint64_t disk_free_bytes) {}

    // Called when DircryptoMigrationProgress signal is received.
    // Typically, called periodicaly during a migration is performed by
    // cryptohomed, as well as to notify the completion of migration.
    virtual void DircryptoMigrationProgress(
        cryptohome::DircryptoMigrationStatus status,
        uint64_t current,
        uint64_t total) {}

   protected:
    virtual ~Observer() = default;
  };

  // Callback for the methods initiate asynchronous operations.
  // On success (i.e. the asynchronous operation is started), an |async_id|
  // is returned, so the user code can identify the corresponding signal
  // handler invocation later.
  using AsyncMethodCallback = DBusMethodCallback<int /* async_id */>;

  // Represents the result to obtain the data related to TPM attestation.
  struct TpmAttestationDataResult {
    // True when it is succeeded to obtain the data.
    bool success = false;

    // The returned content. Available iff |success| is true.
    std::string data;
  };

  // TPM Token Information retrieved from cryptohome.
  // For invalid token |label| and |user_pin| will be empty, while |slot| will
  // be set to -1.
  struct TpmTokenInfo {
    // Holds the PKCS #11 token label. This is not useful in practice to
    // identify a token but may be meaningful to a user.
    std::string label;

    // Can be used with the C_Login PKCS #11 function but is not necessary
    // because tokens are logged in for the duration of a signed-in session.
    std::string user_pin;

    // Corresponds to a CK_SLOT_ID for the PKCS #11 API and reliably
    // identifies the token for the duration of the signed-in session.
    int slot = -1;
  };

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static CryptohomeClient* Get();

  // Returns the sanitized |username| that the stub implementation would return.
  static std::string GetStubSanitizedUsername(
      const cryptohome::AccountIdentifier& id);

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) = 0;

  // Calls IsMounted method and returns true when the call succeeds.
  virtual void IsMounted(DBusMethodCallback<bool> callback) = 0;

  // Calls UnmountEx method. |callback| is called after the method call
  // succeeds.
  virtual void UnmountEx(
      const cryptohome::UnmountRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls MigrateKeyEx method. |callback| is called after the method call
  // succeeds.
  virtual void MigrateKeyEx(
      const cryptohome::AccountIdentifier& account,
      const cryptohome::AuthorizationRequest& auth_request,
      const cryptohome::MigrateKeyRequest& migrate_request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls RemoveEx method.  |callback| is called after the method call
  // succeeds.
  virtual void RemoveEx(const cryptohome::AccountIdentifier& account,
                        DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls RenameCryptohome method. |callback| is called after the method
  // call succeeds.
  virtual void RenameCryptohome(
      const cryptohome::AccountIdentifier& id_from,
      const cryptohome::AccountIdentifier& id_to,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls GetAccountDiskUsage method. |callback| is called after the method
  // call succeeds
  virtual void GetAccountDiskUsage(
      const cryptohome::AccountIdentifier& account_id,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls GetSystemSalt method.  |callback| is called after the method call
  // succeeds.
  virtual void GetSystemSalt(
      DBusMethodCallback<std::vector<uint8_t>> callback) = 0;

  // Calls GetSanitizedUsername method.  |callback| is called after the method
  // call succeeds.
  virtual void GetSanitizedUsername(
      const cryptohome::AccountIdentifier& id,
      DBusMethodCallback<std::string> callback) = 0;

  // Same as GetSanitizedUsername() but blocks until a reply is received, and
  // returns the sanitized username synchronously. Returns an empty string if
  // the method call fails.
  // This may only be called in situations where blocking the UI thread is
  // considered acceptable (e.g. restarting the browser after a crash or after
  // a flag change).
  virtual std::string BlockingGetSanitizedUsername(
      const cryptohome::AccountIdentifier& id) = 0;

  // Calls MountGuestEx method. |callback| is called after the method call
  // succeeds.
  virtual void MountGuestEx(
      const cryptohome::MountGuestRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls GetRsuDeviceId method. |callback| is called after the method call
  // succeeds.
  virtual void GetRsuDeviceId(
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls Pkcs11IsTpmTokenReady method.
  virtual void Pkcs11IsTpmTokenReady(DBusMethodCallback<bool> callback) = 0;

  // Calls Pkcs11GetTpmTokenInfo method.  This method is deprecated, you should
  // use Pkcs11GetTpmTokenInfoForUser instead.  On success |callback| will
  // receive PKCS #11 token information for the token associated with the user
  // who originally signed in (i.e. PKCS #11 slot 0).
  virtual void Pkcs11GetTpmTokenInfo(
      DBusMethodCallback<TpmTokenInfo> callback) = 0;

  // Calls Pkcs11GetTpmTokenInfoForUser method.  On success |callback| will
  // receive PKCS #11 token information for the user identified by |id|.
  virtual void Pkcs11GetTpmTokenInfoForUser(
      const cryptohome::AccountIdentifier& id,
      DBusMethodCallback<TpmTokenInfo> callback) = 0;

  // Calls InstallAttributesGet method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // The original content of |value| is lost.
  virtual bool InstallAttributesGet(const std::string& name,
                                    std::vector<uint8_t>* value,
                                    bool* successful) = 0;

  // Calls InstallAttributesSet method and returns true when the call succeeds.
  // This method blocks until the call returns.
  virtual bool InstallAttributesSet(const std::string& name,
                                    const std::vector<uint8_t>& value,
                                    bool* successful) = 0;

  // Calls InstallAttributesFinalize method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  virtual bool InstallAttributesFinalize(bool* successful) = 0;

  // Calls InstallAttributesIsReady method.
  virtual void InstallAttributesIsReady(DBusMethodCallback<bool> callback) = 0;

  // Calls InstallAttributesIsInvalid method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  virtual bool InstallAttributesIsInvalid(bool* is_invalid) = 0;

  // Calls InstallAttributesIsFirstInstall method and returns true when the call
  // succeeds. This method blocks until the call returns.
  virtual bool InstallAttributesIsFirstInstall(bool* is_first_install) = 0;

  // Asynchronously calls the GetLoginStatus method. |callback| will be invoked
  // with the reply protobuf.
  // GetLoginStatus returns information about the current status of user login.
  // For example, it tells if cryptohome is locked to single user until reboot.
  virtual void GetLoginStatus(
      const cryptohome::GetLoginStatusRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls the GetKeyDataEx method. |callback| will be invoked
  // with the reply protobuf.
  // GetKeyDataEx returns information about the key specified in |request|. At
  // present, this does not include any secret information and the call should
  // not be authenticated (|auth| should be empty).
  virtual void GetKeyDataEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::GetKeyDataRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls CheckKeyEx method. |callback| is called after method
  // call, and with reply protobuf.
  // CheckKeyEx just checks if authorization information is valid.
  virtual void CheckKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::CheckKeyRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls MountEx method. Afterward, |callback| is called with
  // the reply.
  // MountEx attempts to mount home dir using given authorization,
  // and can create new home dir if necessary values are specified in |request|.
  virtual void MountEx(const cryptohome::AccountIdentifier& id,
                       const cryptohome::AuthorizationRequest& auth,
                       const cryptohome::MountRequest& request,
                       DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls DisableLoginUntilReboot method, locking the device
  // into a state where only the user data for provided account_id from
  // |request| can be accessed. After reboot all other user data are accessible.
  virtual void LockToSingleUserMountUntilReboot(
      const cryptohome::LockToSingleUserMountUntilRebootRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls AddKeyEx method. |callback| is called after method
  // call, and with reply protobuf.
  // AddKeyEx adds another key to the given key set. |request| also defines
  // behavior in case when key with specified label already exist.
  virtual void AddKeyEx(const cryptohome::AccountIdentifier& id,
                        const cryptohome::AuthorizationRequest& auth,
                        const cryptohome::AddKeyRequest& request,
                        DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls AddDataRestoreKey method. |callback| is called after
  // method call, and with reply protobuf.
  // AddDataRestoreKey generates data_restore_key in OS and adds it to the
  // given key set. The reply protobuf needs to be extended to
  // AddDataRestoreKeyReply so that caller gets raw bytes of data_restore_key
  virtual void AddDataRestoreKey(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls RemoveKeyEx method. |callback| is called after method
  // call, and with reply protobuf.
  // RemoveKeyEx removes key from the given key set.
  virtual void RemoveKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::RemoveKeyRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls MassRemoveKeys method. |callback| is called after
  // method call, and with reply protobuf.
  // MassRemoveKeys removes all keys except those whose labels are exempted
  // in MassRemoveKeysRequest.
  virtual void MassRemoveKeys(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::MassRemoveKeysRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls StartFingerprintAuthSession method. |callback| is
  // called after method call, and with reply protobuf.
  // StartFingerprintAuthSession prepares biometrics daemon for upcoming
  // fingerprint authentication.
  virtual void StartFingerprintAuthSession(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::StartFingerprintAuthSessionRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls EndFingerprintAuthSession method. |callback| is
  // called after method call, and with reply protobuf.
  // EndFingerprintAuthSession sets biometrics daemon back to normal mode.
  // If there is a reply, it is always an empty reply with no errors.
  virtual void EndFingerprintAuthSession(
      const cryptohome::EndFingerprintAuthSessionRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls GetBootAttribute method. |callback| is called after
  // method call, and with reply protobuf.
  // GetBootAttribute gets the value of the specified boot attribute.
  virtual void GetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls SetBootAttribute method. |callback| is called after
  // method call, and with reply protobuf.
  // SetBootAttribute sets the value of the specified boot attribute. The value
  // won't be available unitl FlushAndSignBootAttributes() is called.
  virtual void SetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls FlushAndSignBootAttributes method. |callback| is
  // called after method call, and with reply protobuf.
  // FlushAndSignBootAttributes makes all pending boot attribute settings
  // available, and have them signed by a special TPM key. This method always
  // fails after any user, publuc, or guest session starts.
  virtual void FlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls MigrateToDircrypto method. It tells cryptohomed to
  // start migration, and is immediately called back by |callback|. The actual
  // result response is done via DircryptoMigrationProgress callback with its
  // status flag indicating the completion.
  // MigrateToDircrypto attempts to migrate the home dir to the new "dircrypto"
  // encryption.
  // |request| contains additional parameters, such as specifying if a full
  // migration or a minimal migration should be performed.
  virtual void MigrateToDircrypto(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::MigrateToDircryptoRequest& request,
      VoidDBusMethodCallback callback) = 0;

  // Asynchronously calls RemoveFirmwareManagementParameters method. |callback|
  // is called after method call, and with reply protobuf.
  virtual void RemoveFirmwareManagementParametersFromTpm(
      const cryptohome::RemoveFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Asynchronously calls SetFirmwareManagementParameters method. |callback|
  // is called after method call, and with reply protobuf. |request| contains
  // the flags to be set. SetFirmwareManagementParameters creates the firmware
  // management parameters in TPM and sets flags included in the request.
  virtual void SetFirmwareManagementParametersInTpm(
      const cryptohome::SetFirmwareManagementParametersRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls NeedsDircryptoMigration to find out whether the given user needs
  // dircrypto migration.
  virtual void NeedsDircryptoMigration(const cryptohome::AccountIdentifier& id,
                                       DBusMethodCallback<bool> callback) = 0;

  // Calls GetSupportedKeyPolicies to determine which type of keys can be added.
  virtual void GetSupportedKeyPolicies(
      const cryptohome::GetSupportedKeyPoliciesRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

  // Calls IsQuotaSupported to know whether quota is supported by cryptohome.
  virtual void IsQuotaSupported(DBusMethodCallback<bool> callback) = 0;

  // Calls GetCurrentSpaceForUid to get the current disk space for an android
  // uid (a shifted uid).
  virtual void GetCurrentSpaceForUid(const uid_t android_uid,
                                     DBusMethodCallback<int64_t> callback) = 0;

  // Calls GetCurrentSpaceForGid to get the current disk space for an android
  // gid (a shifted gid).
  virtual void GetCurrentSpaceForGid(const gid_t android_gid,
                                     DBusMethodCallback<int64_t> callback) = 0;

  // Calls GetCurrentSpaceForProjectId to get the current disk space for a
  // project ID.
  virtual void GetCurrentSpaceForProjectId(
      const int project_id,
      DBusMethodCallback<int64_t> callback) = 0;

  // Calls SetProjectId to set the project ID to the file/directory pointed by
  // path. |parent_path|, |child_path| and |account_id| are used for
  // constructing the target path.
  virtual void SetProjectId(
      const int project_id,
      const cryptohome::SetProjectIdAllowedPathType parent_path,
      const std::string& child_path,
      const cryptohome::AccountIdentifier& account_id,
      DBusMethodCallback<bool> callback) = 0;

  // Calls CheckHealth to get current health state.
  virtual void CheckHealth(
      const cryptohome::CheckHealthRequest& request,
      DBusMethodCallback<cryptohome::BaseReply> callback) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  CryptohomeClient();
  virtual ~CryptohomeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRYPTOHOME_CRYPTOHOME_CLIENT_H_
