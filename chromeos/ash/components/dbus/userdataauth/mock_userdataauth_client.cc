// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"

#include <utility>

#include "base/notreached.h"

namespace ash {

MockUserDataAuthClient::MockUserDataAuthClient() {}
MockUserDataAuthClient::~MockUserDataAuthClient() = default;

void MockUserDataAuthClient::WaitForServiceToBeAvailable(
    chromeos::WaitForServiceToBeAvailableCallback callback) {
  std::move(callback).Run(true);
}

void MockUserDataAuthClient::AddObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::RemoveObserver(Observer* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::AddFingerprintAuthObserver(
    FingerprintAuthObserver* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::RemoveFingerprintAuthObserver(
    FingerprintAuthObserver* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::AddPrepareAuthFactorProgressObserver(
    PrepareAuthFactorProgressObserver* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::RemovePrepareAuthFactorProgressObserver(
    PrepareAuthFactorProgressObserver* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::AddAuthFactorStatusUpdateObserver(
    AuthFactorStatusUpdateObserver* observer) {
  NOTIMPLEMENTED();
}

void MockUserDataAuthClient::RemoveAuthFactorStatusUpdateObserver(
    AuthFactorStatusUpdateObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace ash
