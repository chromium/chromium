// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/userdataauth/mock_userdataauth_client.h"

#include <utility>

#include "base/notreached.h"

namespace chromeos {

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

}  // namespace chromeos
