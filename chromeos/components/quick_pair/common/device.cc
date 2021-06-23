// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_pair/common/device.h"

#include "chromeos/components/quick_pair/common/protocol.h"

namespace chromeos {
namespace quick_pair {

Device::Device(std::string metadata_id, std::string address, Protocol protocol)
    : metadata_id(std::move(metadata_id)),
      address(std::move(address)),
      protocol(protocol) {}

}  // namespace quick_pair
}  // namespace chromeos
