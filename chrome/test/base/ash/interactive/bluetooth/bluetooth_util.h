// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_BLUETOOTH_BLUETOOTH_UTIL_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_BLUETOOTH_BLUETOOTH_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
scoped_refptr<device::BluetoothAdapter> GetBluetoothAdapter();
}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_BLUETOOTH_BLUETOOTH_UTIL_H_
