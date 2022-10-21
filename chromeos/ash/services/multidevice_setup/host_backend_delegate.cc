// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/host_backend_delegate.h"

namespace ash {

namespace multidevice_setup {

HostBackendDelegate::HostBackendDelegate() = default;

HostBackendDelegate::~HostBackendDelegate() = default;

void HostBackendDelegate::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void HostBackendDelegate::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void HostBackendDelegate::NotifyHostChangedOnBackend() {
  for (auto& observer : observer_list_)
    observer.OnHostChangedOnBackend();
}

void HostBackendDelegate::NotifyBackendRequestFailed() {
  for (auto& observer : observer_list_)
    observer.OnBackendRequestFailed();
}

void HostBackendDelegate::NotifyPendingHostRequestChange() {
  for (auto& observer : observer_list_)
    observer.OnPendingHostRequestChange();
}

}  // namespace multidevice_setup

}  // namespace ash
