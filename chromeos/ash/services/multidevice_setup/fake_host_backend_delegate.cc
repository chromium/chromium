// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/fake_host_backend_delegate.h"

#include "base/functional/callback.h"

namespace ash {

namespace multidevice_setup {

FakeHostBackendDelegate::FakeHostBackendDelegate() : HostBackendDelegate() {}

FakeHostBackendDelegate::~FakeHostBackendDelegate() = default;

void FakeHostBackendDelegate::NotifyHostChangedOnBackend(
    const std::optional<multidevice::RemoteDeviceRef>& host_device_on_backend) {
  host_device_on_backend_ = host_device_on_backend;

  if (pending_host_request_ && *pending_host_request_ == host_device_on_backend)
    pending_host_request_.reset();

  HostBackendDelegate::NotifyHostChangedOnBackend();
}

void FakeHostBackendDelegate::NotifyBackendRequestFailed() {
  // A request must be active in order for a back-end request to fail.
  DCHECK(pending_host_request_);

  HostBackendDelegate::NotifyBackendRequestFailed();
}

void FakeHostBackendDelegate::AttemptToSetMultiDeviceHostOnBackend(
    const std::optional<multidevice::RemoteDeviceRef>& host_device) {
  ++num_attempt_to_set_calls_;

  if (host_device_on_backend_ == host_device) {
    if (pending_host_request_) {
      pending_host_request_.reset();
      NotifyPendingHostRequestChange();
    }
    return;
  }

  // If |pending_host_request_| was set and already referred to |host_device|,
  // there is no need to notify observers.
  if (pending_host_request_ && *pending_host_request_ == host_device)
    return;

  pending_host_request_ = host_device;
  NotifyPendingHostRequestChange();
}

bool FakeHostBackendDelegate::HasPendingHostRequest() {
  return pending_host_request_ != std::nullopt;
}

std::optional<multidevice::RemoteDeviceRef>
FakeHostBackendDelegate::GetPendingHostRequest() const {
  return *pending_host_request_;
}

std::optional<multidevice::RemoteDeviceRef>
FakeHostBackendDelegate::GetMultiDeviceHostFromBackend() const {
  return host_device_on_backend_;
}

FakeHostBackendDelegateObserver::FakeHostBackendDelegateObserver() = default;

FakeHostBackendDelegateObserver::~FakeHostBackendDelegateObserver() = default;

void FakeHostBackendDelegateObserver::OnHostChangedOnBackend() {
  ++num_changes_on_backend_;
}

void FakeHostBackendDelegateObserver::OnBackendRequestFailed() {
  ++num_failed_backend_requests_;
}

void FakeHostBackendDelegateObserver::OnPendingHostRequestChange() {
  ++num_pending_host_request_changes_;
}

}  // namespace multidevice_setup

}  // namespace ash
