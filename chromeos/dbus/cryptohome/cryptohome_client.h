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
class CheckKeyRequest;
class FlushAndSignBootAttributesRequest;
class GetBootAttributeRequest;
class GetKeyDataRequest;
class GetSupportedKeyPoliciesRequest;
class GetTpmStatusRequest;
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
class UnmountRequest;
class UpdateKeyRequest;

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

    // Called when TpmInitStatus signal is received, when the status of the TPM
    // initialization is changed.
    virtual void TpmInitStatusUpdated(bool ready,
                                      bool owned,
                                      bool was_owned_this_boot) {}

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

  // Holds TPM version info. Mirrors cryptohome::Tpm::TpmVersionInfo from CrOS
  // side.
  struct TpmVersionInfo {
    uint32_t family = 0;
    uint64_t spec_level = 0;
    uint32_t manufacturer = 0;
    uint32_t tpm_model = 0;
    uint64_t firmware_version = 0;
    std::string vendor_specific;
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

  // Calls TpmIsReady method.
  virtual void TpmIsReady(DBusMethodCallback<bool> callback) = 0;

  // Calls TpmIsEnabled method.
  virtual void TpmIsEnabled(DBusMethodCallback<bool> callback) = 0;

  // Calls TpmIsEnabled method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141006
  virtual bool CallTpmIsEnabledAndBlock(bool* enabled) = 0;

  // Calls TpmGetPassword method.
  virtual void TpmGetPassword(DBusMethodCallback<std::string> callback) = 0;

  // Calls TpmIsOwned method.
  virtual void TpmIsOwned(DBusMethodCallback<bool> callback) = 0;

  // Calls TpmIsOwned method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141012
  virtual bool CallTpmIsOwnedAndBlock(bool* owned) = 0;

  // Calls TpmIsBeingOwned method.
  virtual void TpmIsBeingOwned(DBusMethodCallback<bool> callback) = 0;

  // Calls TpmIsBeingOwned method and returns true when the call succeeds.
  // This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141011
  virtual bool CallTpmIsBeingOwnedAndBlock(bool* owning) = 0;

  // Calls TpmCanAttemptOwnership method.
  // This method tells the service that it is OK to attempt ownership.
  virtual void TpmCanAttemptOwnership(VoidDBusMethodCallback callback) = 0;

  // Calls TpmClearStoredPasswordMethod.
  virtual void TpmClearStoredPassword(VoidDBusMethodCallback callback) = 0;

  // Calls TpmClearStoredPassword method and returns true when the call
  // succeeds.  This method blocks until the call returns.
  // TODO(hashimoto): Remove this method. crbug.com/141010
  virtual bool CallTpmClearStoredPasswordAndBlock() = 0;

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

  // Calls the TpmAttestationIsPrepared dbus method.  The callback is called
  // when the operation completes.
  virtual void TpmAttestationIsPrepared(DBusMethodCallback<bool> callback) = 0;

  // Requests the device's enrollment identifier (EID). The |callback| will be
  // called with the EID. If |ignore_cache| is true, the EID is calculated
  // even if the attestation database already contains a cached version.
  virtual void TpmAttestationGetEnrollmentId(
      bool ignore_cache,
      DBusMethodCallback<TpmAttestationDataResult> callback) = 0;

  // Calls the TpmAttestationIsEnrolled dbus method.  The callback is called
  // when the operation completes.
  virtual void TpmAttestationIsEnrolled(DBusMethodCallback<bool> callback) = 0;

  // Asynchronously creates an attestation enrollment request.  The callback
  // will be called when the dbus call completes.  When the operation completes,
  // the AsyncCallStatusWithDataHandler signal handler is called.  The data that
  // is sent with the signal is an enrollment request to be sent to the Privacy
  // CA of type |pca_type|.  The enrollment is completed by calling
  // AsyncTpmAttestationEnroll.
  virtual void AsyncTpmAttestationCreateEnrollRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      AsyncMethodCallback callback) = 0;

  // Asynchronously finishes an attestation enrollment operation.  The callback
  // will be called when the dbus call completes.  When the operation completes,
  // the AsyncCallStatusHandler signal handler is called.  |pca_response| is the
  // response to the enrollment request emitted by the Privacy CA of type
  // |pca_type|.
  virtual void AsyncTpmAttestationEnroll(
      chromeos::attestation::PrivacyCAType pca_type,
      const std::string& pca_response,
      AsyncMethodCallback callback) = 0;

  // Asynchronously creates an attestation certificate request according to
  // |certificate_profile|.  Some profiles require that the |id| of
  // the currently active user and an identifier of the |request_origin| be
  // provided.  |callback| will be called when the dbus call completes.  When
  // the operation completes, the AsyncCallStatusWithDataHandler signal handler
  // is called.  The data that is sent with the signal is a certificate request
  // to be sent to the Privacy CA of type |pca_type|.  The certificate request
  // is completed by calling AsyncTpmAttestationFinishCertRequest.  The
  // |id| will not be included in the certificate request for the Privacy CA.
  virtual void AsyncTpmAttestationCreateCertRequest(
      chromeos::attestation::PrivacyCAType pca_type,
      attestation::AttestationCertificateProfile certificate_profile,
      const cryptohome::AccountIdentifier& id,
      const std::string& request_origin,
      AsyncMethodCallback callback) = 0;

  // Asynchronously finishes a certificate request operation.  The callback will
  // be called when the dbus call completes.  When the operation completes, the
  // AsyncCallStatusWithDataHandler signal handler is called.  The data that is
  // sent with the signal is a certificate chain in PEM format.  |pca_response|
  // is the response to the certificate request emitted by the Privacy CA.
  // |key_type| determines whether the certified key is to be associated with
  // the current user.  |key_name| is a name for the key.  If |key_type| is
  // KEY_USER, a |id| must be provided.  Otherwise |id| is ignored.
  virtual void AsyncTpmAttestationFinishCertRequest(
      const std::string& pca_response,
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      AsyncMethodCallback callback) = 0;

  // Checks if an attestation key already exists.  If the key specified by
  // |key_type| and |key_name| exists, then the result sent to the callback will
  // be true.  If |key_type| is KEY_USER, a |id| must be provided.
  // Otherwise |id| is ignored.
  virtual void TpmAttestationDoesKeyExist(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<bool> callback) = 0;

  // Gets the attestation certificate for the key specified by |key_type| and
  // |key_name|.  |callback| will be called when the operation completes.  If
  // the key does not exist the callback |result| parameter will be false.  If
  // |key_type| is KEY_USER, a |id| must be provided.  Otherwise |id| is
  // ignored.
  virtual void TpmAttestationGetCertificate(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) = 0;

  // Gets the public key for the key specified by |key_type| and |key_name|.
  // |callback| will be called when the operation completes.  If the key does
  // not exist the callback |result| parameter will be false.  If |key_type| is
  // KEY_USER, a |id| must be provided.  Otherwise |id| is ignored.
  virtual void TpmAttestationGetPublicKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) = 0;

  // Asynchronously registers an attestation key with the current user's
  // PKCS #11 token.  The |callback| will be called when the dbus call
  // completes.  When the operation completes, the AsyncCallStatusHandler signal
  // handler is called.  |key_type| and |key_name| specify the key to register.
  // If |key_type| is KEY_USER, a |id| must be provided.  Otherwise |id| is
  // ignored.
  virtual void TpmAttestationRegisterKey(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      AsyncMethodCallback callback) = 0;

  // Asynchronously signs an enterprise challenge with the key specified by
  // |key_type| and |key_name|.  |domain| and |device_id| will be included in
  // the challenge response.  |options| control how the challenge response is
  // generated.  |challenge| must be a valid enterprise attestation challenge.
  // The |callback| will be called when the dbus call completes.  When the
  // operation completes, the AsyncCallStatusWithDataHandler signal handler is
  // called.  If |key_type| is KEY_USER, a |id| must be provided.
  // Otherwise |id| is ignored. If |key_name_for_spkac| is not empty, then the
  // corresponding key will be used for SignedPublicKeyAndChallenge, but the
  // challenge response will still be signed by the key specified by |key_name|
  // (EMK or EUK).
  virtual void TpmAttestationSignEnterpriseChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      const std::string& domain,
      const std::string& device_id,
      attestation::AttestationChallengeOptions options,
      const std::string& challenge,
      const std::string& key_name_for_spkac,
      AsyncMethodCallback callback) = 0;

  // Asynchronously signs a simple challenge with the key specified by
  // |key_type| and |key_name|.  |challenge| can be any set of arbitrary bytes.
  // A nonce will be appended to the challenge before signing; this method
  // cannot be used to sign arbitrary data.  The |callback| will be called when
  // the dbus call completes.  When the operation completes, the
  // AsyncCallStatusWithDataHandler signal handler is called.  If |key_type| is
  // KEY_USER, a |id| must be provided.  Otherwise |id| is ignored.
  virtual void TpmAttestationSignSimpleChallenge(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      const std::string& challenge,
      AsyncMethodCallback callback) = 0;

  // Gets the payload associated with the key specified by |key_type| and
  // |key_name|.  The |callback| will be called when the operation completes.
  // If the key does not exist the callback |result| parameter will be false.
  // If no payload has been set for the key the callback |result| parameter will
  // be true and the |data| parameter will be empty.  If |key_type| is
  // KEY_USER, a |id| must be provided.  Otherwise |id| is ignored.
  virtual void TpmAttestationGetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      DBusMethodCallback<TpmAttestationDataResult> callback) = 0;

  // Sets the |payload| associated with the key specified by |key_type| and
  // |key_name|.  The |callback| will be called when the operation completes.
  // If the operation succeeds, the callback |result| parameter will be true.
  // If |key_type| is KEY_USER, a |id| must be provided.  Otherwise |id| is
  // ignored.
  virtual void TpmAttestationSetKeyPayload(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_name,
      const std::string& payload,
      DBusMethodCallback<bool> callback) = 0;

  // Deletes certified keys as specified by |key_type| and |key_prefix|.  The
  // |callback| will be called when the operation completes.  If the operation
  // succeeds, the callback |result| parameter will be true.  If |key_type| is
  // KEY_USER, a |id| must be provided.  Otherwise |id| is ignored.
  // All keys where the key name has a prefix matching |key_prefix| will be
  // deleted.  All meta-data associated with the key, including certificates,
  // will also be deleted.
  virtual void TpmAttestationDeleteKeys(
      attestation::AttestationKeyType key_type,
      const cryptohome::AccountIdentifier& id,
      const std::string& key_prefix,
      DBusMethodCallback<bool> callback) = 0;

  // Asynchronously gets the underlying TPM version information and passes it to
  // the given callback.
  virtual void TpmGetVersion(DBusMethodCallback<TpmVersionInfo> callback) = 0;

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

  // Asynchronously calls UpdateKeyEx method. |callback| is called after method
  // call, and with reply protobuf. Reply will contain MountReply extension.
  // UpdateKeyEx replaces key used for authorization, without affecting any
  // other keys. If specified at home dir creation time, new key may have
  // to be signed and/or encrypted.
  virtual void UpdateKeyEx(
      const cryptohome::AccountIdentifier& id,
      const cryptohome::AuthorizationRequest& auth,
      const cryptohome::UpdateKeyRequest& request,
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

  // Asynchronously gets the underlying TPM status information and passes it to
  // the given callback with reply protobuf.
  virtual void GetTpmStatus(
      const cryptohome::GetTpmStatusRequest& request,
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

 protected:
  // Initialize/Shutdown should be used instead.
  CryptohomeClient();
  virtual ~CryptohomeClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptohomeClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CRYPTOHOME_CRYPTOHOME_CLIENT_H_
