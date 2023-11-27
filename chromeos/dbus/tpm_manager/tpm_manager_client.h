// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_TPM_MANAGER_TPM_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_TPM_MANAGER_TPM_MANAGER_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"

namespace dbus {
class Bus;
}

namespace chromeos {

// TpmManagerClient is used to communicate with the org.chromium.TpmManager
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
// For more information, please see
// "chromium/src/third_party/cros_system_api/dbus/tpm_manager" for detailed
// definitions of the D-Bus methods and their arguments.
class COMPONENT_EXPORT(CHROMEOS_DBUS_TPM_MANAGER) TpmManagerClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnOwnershipTaken() = 0;
  };

 public:
  // Callbacks of the D-Bus methods.
  using GetTpmNonsensitiveStatusCallback = base::OnceCallback<void(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply&)>;
  using GetVersionInfoCallback =
      base::OnceCallback<void(const ::tpm_manager::GetVersionInfoReply&)>;
  using GetSupportedFeaturesCallback =
      base::OnceCallback<void(const ::tpm_manager::GetSupportedFeaturesReply&)>;
  using GetDictionaryAttackInfoCallback = base::OnceCallback<void(
      const ::tpm_manager::GetDictionaryAttackInfoReply&)>;
  using TakeOwnershipCallback =
      base::OnceCallback<void(const ::tpm_manager::TakeOwnershipReply&)>;
  using ClearStoredOwnerPasswordCallback = base::OnceCallback<void(
      const ::tpm_manager::ClearStoredOwnerPasswordReply&)>;
  using ClearTpmCallback =
      base::OnceCallback<void(const ::tpm_manager::ClearTpmReply&)>;

  // Interface with testing functionality. Accessed through GetTestInterface(),
  // only implemented in the fake implementation.
  class TestInterface {
   public:
    // Gets a mutable reply that is returned when `GetTpmNonsensitiveStatus()`
    // is called.
    virtual ::tpm_manager::GetTpmNonsensitiveStatusReply*
    mutable_nonsensitive_status_reply() = 0;
    // Sets how many times the `GetTpmNonsensitiveStatus()` returns D-Bus error
    // before it works normally.
    virtual void set_non_nonsensitive_status_dbus_error_count(int count) = 0;
    // Gets a mutable reply that is returned when `GetVersionInfo()` is called.
    virtual ::tpm_manager::GetVersionInfoReply*
    mutable_version_info_reply() = 0;
    // Gets a mutable reply that is returned when `GetSupportedFeatures()` is
    // called.
    virtual ::tpm_manager::GetSupportedFeaturesReply*
    mutable_supported_features_reply() = 0;
    // Gets a mutable reply that is returned when `GetDictionaryAttackInfo()` is
    // called.
    virtual ::tpm_manager::GetDictionaryAttackInfoReply*
    mutable_dictionary_attack_info_reply() = 0;
    // Gets the count of `TakeOwnership()` being called.
    virtual int take_ownership_count() const = 0;
    // Gets the count of `ClearStoredOwnerPassword()` being called.
    virtual int clear_stored_owner_password_count() const = 0;
    // Gets the count of `ClearTpm()` being called.
    virtual int clear_tpm_count() const = 0;
    // Emits ownership taken signal.
    virtual void EmitOwnershipTakenSignal() = 0;
  };

  // Not copyable or movable.
  TpmManagerClient(const TpmManagerClient&) = delete;
  TpmManagerClient& operator=(const TpmManagerClient&) = delete;
  TpmManagerClient(TpmManagerClient&&) = delete;
  TpmManagerClient& operator=(TpmManagerClient&&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static TpmManagerClient* Get();

  // Queries the assorted TPM statuses that tpm manager can tell, e.g., TPM
  // ownership status, the presence of the owner password, the capability of
  // resetting dictionary attack lockout; for the complete list of the returned
  // statuses, see `GetTpmNonsensitiveStatusReply`.
  virtual void GetTpmNonsensitiveStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusRequest& request,
      GetTpmNonsensitiveStatusCallback callback) = 0;
  // Queries the detailed TPM version information; for the complete list of the
  // entries, see `GetVersionInfoReply`.
  virtual void GetVersionInfo(
      const ::tpm_manager::GetVersionInfoRequest& request,
      GetVersionInfoCallback callback) = 0;
  // Queries the features that TPM supports; for the complete list of the
  // entries, see `GetSupportedFeaturesReply`.
  virtual void GetSupportedFeatures(
      const ::tpm_manager::GetSupportedFeaturesRequest& request,
      GetSupportedFeaturesCallback callback) = 0;
  // Queries the dictionary lockout information of TPM's dictionary attack
  // protection, The reply contains the related information, including the
  // current dictionary attack counter, and the flag if the TPM is in the
  // lockout state.
  virtual void GetDictionaryAttackInfo(
      const ::tpm_manager::GetDictionaryAttackInfoRequest& request,
      GetDictionaryAttackInfoCallback callback) = 0;
  // Triggers TPM initialization process by tpm manager.
  virtual void TakeOwnership(const ::tpm_manager::TakeOwnershipRequest& request,
                             TakeOwnershipCallback callback) = 0;
  // Requests tpm manager to attempt to wipe the TPM owner password from its
  // on-disk database.
  virtual void ClearStoredOwnerPassword(
      const ::tpm_manager::ClearStoredOwnerPasswordRequest& request,
      ClearStoredOwnerPasswordCallback callback) = 0;
  // Requests tpm manager to clear TPM after reboot.
  virtual void ClearTpm(const ::tpm_manager::ClearTpmRequest& request,
                        ClearTpmCallback callback) = 0;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;
  // Removes an observer.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  TpmManagerClient();
  virtual ~TpmManagerClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_TPM_MANAGER_TPM_MANAGER_CLIENT_H_
