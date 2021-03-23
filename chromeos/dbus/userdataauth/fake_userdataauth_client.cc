// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"

#include "base/notreached.h"

namespace chromeos {

FakeUserDataAuthClient::FakeUserDataAuthClient() = default;

FakeUserDataAuthClient::~FakeUserDataAuthClient() = default;

void FakeUserDataAuthClient::AddObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void FakeUserDataAuthClient::RemoveObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void FakeUserDataAuthClient::IsMounted(
    const ::user_data_auth::IsMountedRequest& request,
    IsMountedCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUserDataAuthClient::Unmount(
    const ::user_data_auth::UnmountRequest& request,
    UnmountCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::Mount(
    const ::user_data_auth::MountRequest& request,
    MountCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::Remove(
    const ::user_data_auth::RemoveRequest& request,
    RemoveCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::Rename(
    const ::user_data_auth::RenameRequest& request,
    RenameCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::GetKeyData(
    const ::user_data_auth::GetKeyDataRequest& request,
    GetKeyDataCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::CheckKey(
    const ::user_data_auth::CheckKeyRequest& request,
    CheckKeyCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::AddKey(
    const ::user_data_auth::AddKeyRequest& request,
    AddKeyCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::RemoveKey(
    const ::user_data_auth::RemoveKeyRequest& request,
    RemoveKeyCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::MassRemoveKeys(
    const ::user_data_auth::MassRemoveKeysRequest& request,
    MassRemoveKeysCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::MigrateKey(
    const ::user_data_auth::MigrateKeyRequest& request,
    MigrateKeyCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::StartFingerprintAuthSession(
    const ::user_data_auth::StartFingerprintAuthSessionRequest& request,
    StartFingerprintAuthSessionCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::EndFingerprintAuthSession(
    const ::user_data_auth::EndFingerprintAuthSessionRequest& request,
    EndFingerprintAuthSessionCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::StartMigrateToDircrypto(
    const ::user_data_auth::StartMigrateToDircryptoRequest& request,
    StartMigrateToDircryptoCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::NeedsDircryptoMigration(
    const ::user_data_auth::NeedsDircryptoMigrationRequest& request,
    NeedsDircryptoMigrationCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::GetSupportedKeyPolicies(
    const ::user_data_auth::GetSupportedKeyPoliciesRequest& request,
    GetSupportedKeyPoliciesCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::GetAccountDiskUsage(
    const ::user_data_auth::GetAccountDiskUsageRequest& request,
    GetAccountDiskUsageCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::StartAuthSession(
    const ::user_data_auth::StartAuthSessionRequest& request,
    StartAuthSessionCallback callback) {
  NOTIMPLEMENTED();
}
void FakeUserDataAuthClient::AuthenticateAuthSession(
    const ::user_data_auth::AuthenticateAuthSessionRequest& request,
    AuthenticateAuthSessionCallback callback) {
  NOTIMPLEMENTED();
}

void FakeUserDataAuthClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
