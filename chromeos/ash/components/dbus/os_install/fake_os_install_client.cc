// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/os_install/fake_os_install_client.h"

namespace ash {

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

OsInstallClient::TestInterface* FakeOsInstallClient::GetTestInterface() {
  return this;
}

void FakeOsInstallClient::StartOsInstall() {
  NotifyObservers(Status::InProgress, /*service_log=*/"");
}

void FakeOsInstallClient::UpdateStatus(Status status) {
  NotifyObservers(status, /*service_log=*/"");
}

void FakeOsInstallClient::NotifyObservers(Status status,
                                          const std::string& service_log) {
  for (auto& observer : observers_) {
    observer.StatusChanged(status, service_log);
  }
}

}  // namespace ash
