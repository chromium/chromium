// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/nearby_connection.h"

#include "base/memory/ptr_util.h"

namespace chromeos {

namespace secure_channel {

// static
NearbyConnection::Factory* NearbyConnection::Factory::factory_instance_ =
    nullptr;

// static
std::unique_ptr<Connection> NearbyConnection::Factory::Create(
    multidevice::RemoteDeviceRef remote_device) {
  if (factory_instance_)
    return factory_instance_->CreateInstance(remote_device);

  return base::WrapUnique(new NearbyConnection(remote_device));
}

// static
void NearbyConnection::Factory::SetFactoryForTesting(Factory* factory) {
  factory_instance_ = factory;
}

NearbyConnection::NearbyConnection(multidevice::RemoteDeviceRef remote_device)
    : Connection(remote_device) {}

NearbyConnection::~NearbyConnection() {
  // TODO(https://crbug.com/1106937): Clean up potentially-lingering connection.
}

void NearbyConnection::Connect() {
  // TODO(https://crbug.com/1106937): Implement.
}

void NearbyConnection::Disconnect() {
  // TODO(https://crbug.com/1106937): Implement.
}

std::string NearbyConnection::GetDeviceAddress() {
  return remote_device().bluetooth_public_address();
}

void NearbyConnection::SendMessageImpl(std::unique_ptr<WireMessage> message) {
  // TODO(https://crbug.com/1106937): Implement.
}

}  // namespace secure_channel

}  // namespace chromeos
