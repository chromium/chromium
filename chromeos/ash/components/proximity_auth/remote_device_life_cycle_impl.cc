// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/proximity_auth/remote_device_life_cycle_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/proximity_auth/messenger_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace proximity_auth {

namespace {

const char kSmartLockFeatureName[] = "easy_unlock";

}  // namespace

RemoteDeviceLifeCycleImpl::RemoteDeviceLifeCycleImpl(
    ash::multidevice::RemoteDeviceRef remote_device,
    std::optional<ash::multidevice::RemoteDeviceRef> local_device,
    ash::secure_channel::SecureChannelClient* secure_channel_client)
    : remote_device_(remote_device),
      local_device_(local_device),
      secure_channel_client_(secure_channel_client),
      state_(RemoteDeviceLifeCycle::State::STOPPED) {}

RemoteDeviceLifeCycleImpl::~RemoteDeviceLifeCycleImpl() {}

void RemoteDeviceLifeCycleImpl::Start() {
  PA_LOG(VERBOSE) << "Life cycle for " << remote_device_.name() << " started.";
  DCHECK(state_ == RemoteDeviceLifeCycle::State::STOPPED);
  FindConnection();
}

ash::multidevice::RemoteDeviceRef RemoteDeviceLifeCycleImpl::GetRemoteDevice()
    const {
  return remote_device_;
}

ash::secure_channel::ClientChannel* RemoteDeviceLifeCycleImpl::GetChannel()
    const {
  if (channel_)
    return channel_.get();
  if (messenger_)
    return messenger_->GetChannel();
  return nullptr;
}

RemoteDeviceLifeCycle::State RemoteDeviceLifeCycleImpl::GetState() const {
  return state_;
}

Messenger* RemoteDeviceLifeCycleImpl::GetMessenger() {
  return messenger_.get();
}

void RemoteDeviceLifeCycleImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RemoteDeviceLifeCycleImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RemoteDeviceLifeCycleImpl::TransitionToState(
    RemoteDeviceLifeCycle::State new_state) {
  PA_LOG(VERBOSE) << "Life cycle transition: " << state_ << " => " << new_state;
  RemoteDeviceLifeCycle::State old_state = state_;
  state_ = new_state;
  for (auto& observer : observers_)
    observer.OnLifeCycleStateChanged(old_state, new_state);
}

void RemoteDeviceLifeCycleImpl::FindConnection() {
  connection_attempt_ = secure_channel_client_->ListenForConnectionFromDevice(
      remote_device_, *local_device_, kSmartLockFeatureName,
      ash::secure_channel::ConnectionMedium::kBluetoothLowEnergy,
      ash::secure_channel::ConnectionPriority::kHigh);
  connection_attempt_->SetDelegate(this);

  TransitionToState(RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
}

void RemoteDeviceLifeCycleImpl::CreateMessenger() {
  DCHECK(state_ == RemoteDeviceLifeCycle::State::AUTHENTICATING);

  messenger_ = std::make_unique<MessengerImpl>(std::move(channel_));
  messenger_->AddObserver(this);

  TransitionToState(RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);
}

void RemoteDeviceLifeCycleImpl::OnConnectionAttemptFailure(
    ash::secure_channel::mojom::ConnectionAttemptFailureReason reason) {
  connection_attempt_.reset();

  if (reason == ash::secure_channel::mojom::ConnectionAttemptFailureReason::
                    ADAPTER_DISABLED ||
      reason == ash::secure_channel::mojom::ConnectionAttemptFailureReason::
                    ADAPTER_NOT_PRESENT) {
    // Transition to state STOPPED, and wait for Bluetooth to become powered.
    // If it does, UnlockManager will start RemoteDeviceLifeCycle again.
    PA_LOG(WARNING) << "Life cycle for "
                    << remote_device_.GetTruncatedDeviceIdForLogs()
                    << " stopped because Bluetooth is not available.";
    TransitionToState(RemoteDeviceLifeCycle::State::STOPPED);
  } else {
    // TODO(crbug.com/41474905): Improve the name AUTHENTICATION_FAILED (it can
    // encompass errors other than authentication failures) and create a metric
    // with buckets corresponding to the ConnectionAttemptFailureReason.
    PA_LOG(ERROR) << "Failed to authenticate with remote device: "
                  << remote_device_.GetTruncatedDeviceIdForLogs()
                  << ", for reason: " << reason << ". Giving up.";
    TransitionToState(RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED);
  }
}

void RemoteDeviceLifeCycleImpl::OnConnection(
    std::unique_ptr<ash::secure_channel::ClientChannel> channel) {
  DCHECK(state_ == RemoteDeviceLifeCycle::State::FINDING_CONNECTION);
  TransitionToState(RemoteDeviceLifeCycle::State::AUTHENTICATING);

  channel_ = std::move(channel);

  // Create the MessengerImpl asynchronously. |messenger_| registers itself as
  // an observer of |channel_|, so creating it synchronously would trigger
  // |OnSendCompleted()| as an observer call for |messenger_|.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RemoteDeviceLifeCycleImpl::CreateMessenger,
                                weak_ptr_factory_.GetWeakPtr()));
}

void RemoteDeviceLifeCycleImpl::OnDisconnected() {
  DCHECK(state_ == RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED);

  messenger_->RemoveObserver(this);
  messenger_.reset();

  FindConnection();
}

}  // namespace proximity_auth
