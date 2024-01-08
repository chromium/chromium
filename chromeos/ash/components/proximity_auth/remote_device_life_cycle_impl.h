// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_LIFE_CYCLE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_LIFE_CYCLE_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/proximity_auth/messenger_observer.h"
#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash {
namespace secure_channel {
class ClientChannel;
class SecureChannelClient;
}  // namespace secure_channel
}  // namespace ash

namespace proximity_auth {

class Messenger;

// Implementation of RemoteDeviceLifeCycle.
class RemoteDeviceLifeCycleImpl
    : public RemoteDeviceLifeCycle,
      public ash::secure_channel::ConnectionAttempt::Delegate,
      public MessengerObserver {
 public:
  // Creates the life cycle for controlling the given |remote_device|.
  // |proximity_auth_client| is not owned.
  RemoteDeviceLifeCycleImpl(
      ash::multidevice::RemoteDeviceRef remote_device,
      std::optional<ash::multidevice::RemoteDeviceRef> local_device,
      ash::secure_channel::SecureChannelClient* secure_channel_client);

  RemoteDeviceLifeCycleImpl(const RemoteDeviceLifeCycleImpl&) = delete;
  RemoteDeviceLifeCycleImpl& operator=(const RemoteDeviceLifeCycleImpl&) =
      delete;

  ~RemoteDeviceLifeCycleImpl() override;

  // RemoteDeviceLifeCycle:
  void Start() override;
  ash::multidevice::RemoteDeviceRef GetRemoteDevice() const override;
  ash::secure_channel::ClientChannel* GetChannel() const override;

  RemoteDeviceLifeCycle::State GetState() const override;
  Messenger* GetMessenger() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  // Transitions to |new_state|, and notifies observers.
  void TransitionToState(RemoteDeviceLifeCycle::State new_state);

  // Transtitions to FINDING_CONNECTION state. Creates and starts
  // |connection_finder_|.
  void FindConnection();

  // Creates the messenger which parses status updates.
  void CreateMessenger();

  // ash::secure_channel::ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      ash::secure_channel::mojom::ConnectionAttemptFailureReason reason)
      override;
  void OnConnection(
      std::unique_ptr<ash::secure_channel::ClientChannel> channel) override;

  // MessengerObserver:
  void OnDisconnected() override;

  // The remote device being controlled.
  const ash::multidevice::RemoteDeviceRef remote_device_;

  // Represents this device (i.e. this Chromebook) for a particular profile.
  std::optional<ash::multidevice::RemoteDeviceRef> local_device_;

  // The entrypoint to the SecureChannel API.
  raw_ptr<ash::secure_channel::SecureChannelClient> secure_channel_client_;

  // The current state in the life cycle.
  RemoteDeviceLifeCycle::State state_;

  // Observers added to the life cycle.
  base::ObserverList<Observer>::Unchecked observers_{
      base::ObserverListPolicy::EXISTING_ONLY};

  // The messenger for sending and receiving messages in the
  // SECURE_CHANNEL_ESTABLISHED state.
  std::unique_ptr<Messenger> messenger_;

  std::unique_ptr<ash::secure_channel::ConnectionAttempt> connection_attempt_;

  // Ownership is eventually passed to |messenger_|.
  std::unique_ptr<ash::secure_channel::ClientChannel> channel_;

  // After authentication fails, this timer waits for a period of time before
  // retrying the connection.
  base::OneShotTimer authentication_recovery_timer_;

  base::WeakPtrFactory<RemoteDeviceLifeCycleImpl> weak_ptr_factory_{this};
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_REMOTE_DEVICE_LIFE_CYCLE_IMPL_H_
