// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_

namespace chromeos {

namespace device_sync {

// Creates a stub DeviceSync factory that initializes a stub DeviceSync, then
// sets that factory as the DeviceSyncImpl custom factory.
void SetStubDeviceSyncFactory();

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_STUB_DEVICE_SYNC_H_
