// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_REMOTE_DEVICE_LIFE_CYCLE_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_REMOTE_DEVICE_LIFE_CYCLE_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::secure_channel {
class ClientChannel;
}

namespace proximity_auth {

class FakeRemoteDeviceLifeCycle : public RemoteDeviceLifeCycle {
 public:
  explicit FakeRemoteDeviceLifeCycle(
      ash::multidevice::RemoteDeviceRef remote_device,
      absl::optional<ash::multidevice::RemoteDeviceRef> local_device);

  FakeRemoteDeviceLifeCycle(const FakeRemoteDeviceLifeCycle&) = delete;
  FakeRemoteDeviceLifeCycle& operator=(const FakeRemoteDeviceLifeCycle&) =
      delete;

  ~FakeRemoteDeviceLifeCycle() override;

  // RemoteDeviceLifeCycle:
  void Start() override;
  ash::multidevice::RemoteDeviceRef GetRemoteDevice() const override;
  ash::secure_channel::ClientChannel* GetChannel() const override;
  State GetState() const override;
  Messenger* GetMessenger() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Changes state and notifies observers.
  void ChangeState(State new_state);

  void set_messenger(Messenger* messenger) { messenger_ = messenger; }

  void set_channel(ash::secure_channel::ClientChannel* channel) {
    channel_ = channel;
  }

  bool started() { return started_; }

  ash::multidevice::RemoteDeviceRef local_device() { return *local_device_; }

  base::ObserverList<Observer>::Unchecked& observers() { return observers_; }

 private:
  ash::multidevice::RemoteDeviceRef remote_device_;
  absl::optional<ash::multidevice::RemoteDeviceRef> local_device_;
  base::ObserverList<Observer>::Unchecked observers_;
  bool started_;
  State state_;
  raw_ptr<ash::secure_channel::ClientChannel,
          DanglingUntriaged | ExperimentalAsh>
      channel_;
  raw_ptr<Messenger, ExperimentalAsh> messenger_;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_FAKE_REMOTE_DEVICE_LIFE_CYCLE_H_
