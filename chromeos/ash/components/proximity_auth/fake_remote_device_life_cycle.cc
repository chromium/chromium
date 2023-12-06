// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/fake_remote_device_life_cycle.h"

#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"

namespace proximity_auth {

FakeRemoteDeviceLifeCycle::FakeRemoteDeviceLifeCycle(
    ash::multidevice::RemoteDeviceRef remote_device,
    std::optional<ash::multidevice::RemoteDeviceRef> local_device)
    : remote_device_(remote_device),
      local_device_(local_device),
      started_(false),
      state_(RemoteDeviceLifeCycle::State::STOPPED) {}

FakeRemoteDeviceLifeCycle::~FakeRemoteDeviceLifeCycle() = default;

void FakeRemoteDeviceLifeCycle::Start() {
  started_ = true;
  ChangeState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
}

ash::multidevice::RemoteDeviceRef FakeRemoteDeviceLifeCycle::GetRemoteDevice()
    const {
  return remote_device_;
}

ash::secure_channel::ClientChannel* FakeRemoteDeviceLifeCycle::GetChannel()
    const {
  return channel_;
}

RemoteDeviceLifeCycle::State FakeRemoteDeviceLifeCycle::GetState() const {
  return state_;
}

Messenger* FakeRemoteDeviceLifeCycle::GetMessenger() {
  return messenger_;
}

void FakeRemoteDeviceLifeCycle::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeRemoteDeviceLifeCycle::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeRemoteDeviceLifeCycle::ChangeState(State new_state) {
  State old_state = state_;
  state_ = new_state;
  for (auto& observer : observers_)
    observer.OnLifeCycleStateChanged(old_state, new_state);
}

}  // namespace proximity_auth
