// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_TPM_MANAGER_FAKE_TPM_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_TPM_MANAGER_FAKE_TPM_MANAGER_CLIENT_H_

#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"

#include "base/component_export.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_DBUS_TPM_MANAGER) FakeTpmManagerClient
    : public TpmManagerClient,
      public TpmManagerClient::TestInterface {
 public:
  FakeTpmManagerClient();
  ~FakeTpmManagerClient() override;

  // Not copyable or movable.
  FakeTpmManagerClient(const FakeTpmManagerClient&) = delete;
  FakeTpmManagerClient& operator=(const FakeTpmManagerClient&) = delete;
  FakeTpmManagerClient(FakeTpmManagerClient&&) = delete;
  FakeTpmManagerClient& operator=(FakeTpmManagerClient&&) = delete;

  // TpmManagerClient:
  void GetTpmNonsensitiveStatus(
      const ::tpm_manager::GetTpmNonsensitiveStatusRequest& request,
      GetTpmNonsensitiveStatusCallback callback) override;
  void GetVersionInfo(const ::tpm_manager::GetVersionInfoRequest& request,
                      GetVersionInfoCallback callback) override;
  void GetDictionaryAttackInfo(
      const ::tpm_manager::GetDictionaryAttackInfoRequest& request,
      GetDictionaryAttackInfoCallback callback) override;
  void TakeOwnership(const ::tpm_manager::TakeOwnershipRequest& request,
                     TakeOwnershipCallback callback) override;
  void ClearStoredOwnerPassword(
      const ::tpm_manager::ClearStoredOwnerPasswordRequest& request,
      ClearStoredOwnerPasswordCallback callback) override;

  TpmManagerClient::TestInterface* GetTestInterface() override;

  // TpmManagerClient::TestInterface:
  ::tpm_manager::GetTpmNonsensitiveStatusReply*
  mutable_nonsensitive_status_reply() override;
  ::tpm_manager::GetVersionInfoReply* mutable_version_info_reply() override;
  int clear_stored_owner_password_count() const override;

 private:
  ::tpm_manager::GetTpmNonsensitiveStatusReply nonsensitive_status_reply_;
  ::tpm_manager::GetVersionInfoReply version_info_reply_;
  int clear_stored_owner_password_count_ = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_TPM_MANAGER_FAKE_TPM_MANAGER_CLIENT_H_
