// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/mock_remote_device.h"

namespace chromecast {
namespace bluetooth {

MockRemoteDevice::MockRemoteDevice(const bluetooth_v2_shlib::Addr& addr)
    : addr_(addr) {}

MockRemoteDevice::~MockRemoteDevice() = default;

}  // namespace bluetooth
}  // namespace chromecast
