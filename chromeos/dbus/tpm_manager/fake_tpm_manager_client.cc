// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/tpm_manager/fake_tpm_manager_client.h"

#include "base/notreached.h"

namespace chromeos {

FakeTpmManagerClient::FakeTpmManagerClient() = default;

FakeTpmManagerClient::~FakeTpmManagerClient() = default;

void FakeTpmManagerClient::GetTpmNonsensitiveStatus(
    const ::tpm_manager::GetTpmNonsensitiveStatusRequest& request,
    GetTpmNonsensitiveStatusCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::GetVersionInfo(
    const ::tpm_manager::GetVersionInfoRequest& request,
    GetVersionInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::GetDictionaryAttackInfo(
    const ::tpm_manager::GetDictionaryAttackInfoRequest& request,
    GetDictionaryAttackInfoCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::TakeOwnership(
    const ::tpm_manager::TakeOwnershipRequest& request,
    TakeOwnershipCallback callback) {
  NOTIMPLEMENTED();
}

void FakeTpmManagerClient::ClearStoredOwnerPassword(
    const ::tpm_manager::ClearStoredOwnerPasswordRequest& request,
    ClearStoredOwnerPasswordCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
