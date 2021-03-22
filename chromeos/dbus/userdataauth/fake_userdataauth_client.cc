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
void FakeUserDataAuthClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace chromeos
