// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file just instantiates mocks to verify they can compile since no one
// uses them yet.

#include "chromecast/device/bluetooth/le/mock_gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/mock_le_scan_manager.h"
#include "chromecast/device/bluetooth/le/mock_remote_characteristic.h"
#include "chromecast/device/bluetooth/le/mock_remote_descriptor.h"
#include "chromecast/device/bluetooth/le/mock_remote_device.h"
#include "chromecast/device/bluetooth/le/mock_remote_service.h"

namespace chromecast {
namespace bluetooth {

namespace {

const bluetooth_v2_shlib::Addr kAddr{};
const bluetooth_v2_shlib::Uuid kUuid{};

}  // namespace

void InstantiateMocks() {
  MockGattClientManager a;
  MockLeScanManager b;
  scoped_refptr<MockRemoteCharacteristic> c(
      new MockRemoteCharacteristic(kUuid));
  scoped_refptr<MockRemoteDescriptor> d(new MockRemoteDescriptor);
  scoped_refptr<MockRemoteDevice> e(new MockRemoteDevice(kAddr));
  scoped_refptr<MockRemoteService> f(new MockRemoteService(kUuid));
}

}  // namespace bluetooth
}  // namespace chromecast
