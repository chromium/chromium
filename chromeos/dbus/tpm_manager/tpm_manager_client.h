// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_TPM_MANAGER_TPM_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_TPM_MANAGER_TPM_MANAGER_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
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
  using GetTpmNonsensitiveStatusCallback = base::OnceCallback<void(
      const ::tpm_manager::GetTpmNonsensitiveStatusReply&)>;
  using GetVersionInfoCallback =
      base::OnceCallback<void(const ::tpm_manager::GetVersionInfoReply&)>;
  using GetDictionaryAttackInfoCallback = base::OnceCallback<void(
      const ::tpm_manager::GetDictionaryAttackInfoReply&)>;
  using TakeOwnershipCallback =
      base::OnceCallback<void(const ::tpm_manager::TakeOwnershipReply&)>;
  using ClearStoredOwnerPasswordCallback = base::OnceCallback<void(
      const ::tpm_manager::ClearStoredOwnerPasswordReply&)>;

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

 protected:
  // Initialize/Shutdown should be used instead.
  TpmManagerClient();
  virtual ~TpmManagerClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_TPM_MANAGER_TPM_MANAGER_CLIENT_H_
