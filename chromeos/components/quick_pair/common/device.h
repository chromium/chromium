// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_PAIR_COMMON_DEVICE_H_
#define CHROMEOS_COMPONENTS_QUICK_PAIR_COMMON_DEVICE_H_

#include "chromeos/components/quick_pair/common/protocol.h"

namespace chromeos {
namespace quick_pair {

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a device.
//
// Lower level components will use |protocol|, |metadata_id| and |address| to
// fetch objects which contain more information. E.g. A Fast Pair component
// can use |metadata_id| to query the Service to receive a full metadata object.
struct Device {
  Device(std::string metadata_id, std::string address, Protocol protocol);
  Device(const Device&) = default;
  Device(Device&&) = default;
  Device& operator=(const Device&) = delete;
  Device& operator=(Device&&) = delete;
  ~Device() = default;

  // An identifier which components can use to fetch additional metadata for
  // this device. This ID will correspond to different things depending on
  // |protocol|. For example, if |protocol| is Fast Pair, this ID will be the
  // model ID of the Fast Pair device.
  const std::string metadata_id;

  // Bluetooth address of the device.
  const std::string address;

  // The Quick Pair protocol implementation that this device belongs to.
  const Protocol protocol;
};

}  // namespace quick_pair
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_QUICK_PAIR_COMMON_DEVICE_H_
