// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_INSTALL_ATTRIBUTES_INSTALL_ATTRIBUTES_H_
#define CHROMEOS_ASH_COMPONENTS_INSTALL_ATTRIBUTES_INSTALL_ATTRIBUTES_H_

#include <map>
#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/device_management/install_attributes_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace ash {

// Brokers access to the installation-time attributes on Chrome OS.  When
// initialized with kInstallAttributesFileName, the attributes are fully trusted
// (signature has been verified by lockbox-cache).
class COMPONENT_EXPORT(ASH_INSTALL_ATTRIBUTES) InstallAttributes {
 public:
  // InstallAttributes status codes.  Do not change the numeric ids or the
  // meaning of the existing codes to preserve the interpretability of old
  // logfiles.
  enum LockResult {
    LOCK_SUCCESS = 0,          // Success.
    LOCK_NOT_READY = 1,        // Backend/TPM still initializing.
    LOCK_TIMEOUT = 2,          // Backend/TPM timed out.
    LOCK_BACKEND_INVALID = 3,  // Backend failed to initialize.
    LOCK_ALREADY_LOCKED = 4,   // TPM has already been locked.
    LOCK_SET_ERROR = 5,        // Failed to set attributes.
    LOCK_FINALIZE_ERROR = 6,   // Backend failed to lock.
    LOCK_READBACK_ERROR = 7,   // Inconsistency reading back registration data.
    LOCK_WRONG_DOMAIN = 8,     // Device already registered to another domain or
                               // other mismatch of other attributes.
    LOCK_WRONG_MODE = 9,       // Device already locked to a different mode.
  };

  // A callback to handle responses of methods returning a LockResult value.
  using LockResultCallback = base::OnceCallback<void(LockResult lock_result)>;

  // Manage singleton instance.
  static void Initialize();
  static bool IsInitialized();
  static void Shutdown();
  static InstallAttributes* Get();

  // Sets the singleton to |test_instance|. Does not take ownership of the
  // instance. Should be matched with a call to |ShutdownForTesting| once the
  // test is finished and before the instance is deleted.
  static void SetForTesting(InstallAttributes* test_instance);
  static void ShutdownForTesting();

  explicit InstallAttributes(
      InstallAttributesClient* install_attributes_client);

  InstallAttributes(const InstallAttributes&) = delete;
  InstallAttributes& operator=(const InstallAttributes&) = delete;

  ~InstallAttributes();

  // Tries to read install attributes from |cache_file| to work around slow
  // cryptohome startup which takes a while to register its D-Bus interface.
  // (See http://crosbug.com/37367 for background on this.)  When called with
  // kInstallAttributesFileName (created early during the boot process by
  // lockbox-cache) the install attributes are fully trusted.
  void Init(const base::FilePath& cache_file);

  // Makes sure the local caches for enterprise-related install attributes are
  // up to date with what cryptohome has. This method checks the readiness of
  // attributes and read them if ready. Actual read will be performed in
  // ReadAttributesIfReady().
  void ReadImmutableAttributes(base::OnceClosure callback);

  // Updates the firmware management parameters from TPM, storing the devmode
  // flag according to |block_devmode|. Invokes |callback| when done. Must be
  // called before LockDevice is done. Used to update TPM on enrollment.
  void SetBlockDevmodeInTpm(
      bool block_devmode,
      chromeos::DBusMethodCallback<
          device_management::SetFirmwareManagementParametersReply> callback);

  // Locks the device into |device_mode|.  Depending on |device_mode|, a
  // specific subset of |domain|, |realm| and |device_id| must be set.  Can also
  // be called after the lock has already been taken, in which case it checks
  // that the passed parameters fully agree with the locked attributes.
  // |callback| must not be null and is called with the result.  Must not be
  // called while a previous LockDevice() invocation is still pending.
  void LockDevice(policy::DeviceMode device_mode,
                  const std::string& domain,
                  const std::string& realm,
                  const std::string& device_id,
                  LockResultCallback callback);

  // Checks whether this devices is under any kind of enterprise management.
  bool IsEnterpriseManaged() const;

  // Checks whether this is a cloud (DM server) managed enterprise device.
  bool IsCloudManaged() const;

  // Whether the device is set up to run demo sessions.
  bool IsDeviceInDemoMode() const;

  // Return the mode the device was enrolled to. The return value for devices
  // that are not locked yet is DEVICE_MODE_UNKNOWN.
  policy::DeviceMode GetMode() const { return registration_mode_; }

  // Return the domain this device belongs to or an empty string if the device
  // is not a cloud-managed enterprise device.
  std::string GetDomain() const { return registration_domain_; }

  // Return the realm this device belongs to or an empty string if the device is
  // not an AD enterprise device.
  std::string GetRealm() const { return registration_realm_; }

  // Return the device id that was generated when the device was registered.
  // Returns an empty string if the device is not an enterprise device or the
  // device id was not stored in the lockbox (prior to R19).
  std::string GetDeviceId() const { return registration_device_id_; }

  // Return whether TPM is locked.
  bool IsDeviceLocked() const { return device_locked_; }

 protected:
  // True if install attributes have been read successfully.  False if read
  // failed or no read attempt was made.
  bool device_locked_ = false;

  // Whether the TPM / install attributes consistency check is running.
  bool consistency_check_running_ = false;

  // To be run after the consistency check has finished.
  base::OnceClosure post_check_action_;

  // Wether the LockDevice() initiated TPM calls are running.
  bool device_lock_running_ = false;

  // The actual install attributes.  Populated by DecodeInstallAttributes()
  // exclusively.
  policy::DeviceMode registration_mode_ = policy::DEVICE_MODE_PENDING;
  std::string registration_domain_;
  std::string registration_realm_;
  std::string registration_device_id_;

 private:
  FRIEND_TEST_ALL_PREFIXES(InstallAttributesTest, DeviceLockedFromOlderVersion);
  FRIEND_TEST_ALL_PREFIXES(InstallAttributesTest, Init);
  FRIEND_TEST_ALL_PREFIXES(InstallAttributesTest, InitForConsumerKiosk);
  FRIEND_TEST_ALL_PREFIXES(InstallAttributesTest, InitForEnterpriseDemo);
  FRIEND_TEST_ALL_PREFIXES(InstallAttributesTest, LockCanonicalize);
  FRIEND_TEST_ALL_PREFIXES(InstallAttributesTest,
                           VerifyFakeInstallAttributesCache);

  // Constants for the possible device modes that can be stored in the lockbox.
  static const char kConsumerDeviceMode[];
  static const char kEnterpriseDeviceMode[];
  static const char kLegacyRetailDeviceMode[];
  static const char kLegacyConsumerKioskDeviceMode[];
  static const char kDemoDeviceMode[];

  // Field names in the lockbox.
  static const char kAttrEnterpriseDeviceId[];
  static const char kAttrEnterpriseDomain[];
  static const char kAttrEnterpriseRealm[];
  static const char kAttrEnterpriseMode[];
  static const char kAttrEnterpriseOwned[];
  static const char kAttrEnterpriseUser[];
  static const char kAttrConsumerKioskEnabled[];

  // Called by |cryptohome_client_| when the cryptohome service becomes
  // initially available over D-Bus.
  void OnCryptohomeServiceInitiallyAvailable(bool service_is_ready);

  // Translates DeviceMode constants to strings used in the lockbox.
  std::string GetDeviceModeString(policy::DeviceMode mode);

  // Translates strings used in the lockbox to DeviceMode values.
  policy::DeviceMode GetDeviceModeFromString(const std::string& mode);

  // Decode the install attributes provided in |attr_map| (including some
  // normalization and processing for backward compatibility) and guarantee that
  // |registration_*| members are set self-consistently.
  void DecodeInstallAttributes(
      const std::map<std::string, std::string>& attr_map);

  // Helper for ReadImmutableAttributes.
  void ReadAttributesIfReady(
      base::OnceClosure callback,
      std::optional<device_management::InstallAttributesGetStatusReply> reply);

  // Helper for LockDevice(). Handles the result of InstallAttributesIsReady()
  // and continue processing LockDevice if the result is true.
  void LockDeviceIfAttributesIsReady(
      policy::DeviceMode device_mode,
      const std::string& domain,
      const std::string& realm,
      const std::string& device_id,
      LockResultCallback callback,
      std::optional<device_management::InstallAttributesGetStatusReply> reply);

  // Confirms the registered user and invoke the callback.
  void OnReadImmutableAttributes(policy::DeviceMode mode,
                                 const std::string& domain,
                                 const std::string& realm,
                                 const std::string& device_id,
                                 LockResultCallback callback);

  // Check state of install attributes against TPM lock state and generate UMA
  // for the result.  Asynchronously retry |dbus_retries| times in case of DBUS
  // errors (cryptohomed startup is slow).
  void TriggerConsistencyCheck(int dbus_retries);

  // Callback for `GetTpmNonsensitiveStatus()` D-Bus call. Generates UMA or
  // schedules retry in case of DBUS error.
  void OnTpmStatusComplete(
      int dbus_retries_remaining,
      const ::tpm_manager::GetTpmNonsensitiveStatusReply& reply);

  // Callback for `ClearStoredOwnerPassword()` D-Bus call. Logs status code if
  // the call fails.
  void OnClearStoredOwnerPassword(
      const ::tpm_manager::ClearStoredOwnerPasswordReply& reply);

  raw_ptr<InstallAttributesClient, DanglingUntriaged>
      install_attributes_client_;

  base::WeakPtrFactory<InstallAttributes> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_INSTALL_ATTRIBUTES_INSTALL_ATTRIBUTES_H_
