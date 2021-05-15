// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_FAKE_REMOTE_DEVICE_LIFE_CYCLE_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_FAKE_REMOTE_DEVICE_LIFE_CYCLE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/services/secure_channel/fake_connection.h"
#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"

namespace proximity_auth {

class FakeRemoteDeviceLifeCycle : public RemoteDeviceLifeCycle {
 public:
  explicit FakeRemoteDeviceLifeCycle(
      chromeos::multidevice::RemoteDeviceRef remote_device,
      absl::optional<chromeos::multidevice::RemoteDeviceRef> local_device);
  ~FakeRemoteDeviceLifeCycle() override;

  // RemoteDeviceLifeCycle:
  void Start() override;
  chromeos::multidevice::RemoteDeviceRef GetRemoteDevice() const override;
  chromeos::secure_channel::ClientChannel* GetChannel() const override;
  State GetState() const override;
  Messenger* GetMessenger() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Changes state and notifies observers.
  void ChangeState(State new_state);

  void set_messenger(Messenger* messenger) { messenger_ = messenger; }

  void set_channel(chromeos::secure_channel::ClientChannel* channel) {
    channel_ = channel;
  }

  bool started() { return started_; }

  chromeos::multidevice::RemoteDeviceRef local_device() {
    return *local_device_;
  }

  base::ObserverList<Observer>::Unchecked& observers() { return observers_; }

 private:
  chromeos::multidevice::RemoteDeviceRef remote_device_;
  absl::optional<chromeos::multidevice::RemoteDeviceRef> local_device_;
  base::ObserverList<Observer>::Unchecked observers_;
  bool started_;
  State state_;
  chromeos::secure_channel::ClientChannel* channel_;
  Messenger* messenger_;

  DISALLOW_COPY_AND_ASSIGN(FakeRemoteDeviceLifeCycle);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_FAKE_REMOTE_DEVICE_LIFE_CYCLE_H_
