// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/bluetooth/bluetooth_util.h"

#include "base/test/test_future.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {

scoped_refptr<device::BluetoothAdapter> GetBluetoothAdapter() {
  base::test::TestFuture<scoped_refptr<device::BluetoothAdapter>> adapter;
  device::BluetoothAdapterFactory::Get()->GetAdapter(adapter.GetCallback());
  return adapter.Take();
}

}  // namespace ash
