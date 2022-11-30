// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/remote_descriptor.h"

namespace chromecast {
namespace bluetooth {

// static
constexpr uint8_t RemoteDescriptor::kEnableNotificationValue[];
// static
constexpr uint8_t RemoteDescriptor::kEnableIndicationValue[];
// static
constexpr uint8_t RemoteDescriptor::kDisableNotificationValue[];
// static
const bluetooth_v2_shlib::Uuid RemoteDescriptor::kCccdUuid = {
    {0x00, 0x00, 0x29, 0x02, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80,
     0x5f, 0x9b, 0x34, 0xfb}};

}  // namespace bluetooth
}  // namespace chromecast
