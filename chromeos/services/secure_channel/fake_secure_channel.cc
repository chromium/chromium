// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_secure_channel.h"

#include "base/memory/ptr_util.h"

namespace chromeos {

namespace secure_channel {

FakeSecureChannel::FakeSecureChannel() = default;

FakeSecureChannel::~FakeSecureChannel() = default;

void FakeSecureChannel::ListenForConnectionFromDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  delegate_from_last_listen_call_.Bind(std::move(delegate));
}

void FakeSecureChannel::InitiateConnectionToDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  delegate_from_last_initiate_call_.Bind(std::move(delegate));
}

}  // namespace secure_channel

}  // namespace chromeos
