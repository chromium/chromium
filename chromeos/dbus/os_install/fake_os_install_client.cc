// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/os_install/fake_os_install_client.h"

namespace chromeos {

FakeOsInstallClient::FakeOsInstallClient() = default;
FakeOsInstallClient::~FakeOsInstallClient() = default;

void FakeOsInstallClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeOsInstallClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeOsInstallClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeOsInstallClient::StartOsInstall(StartOsInstallCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

}  // namespace chromeos
