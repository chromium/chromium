// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/device_id.h"

namespace policy {

DeviceId::DeviceId(uint16_t vid, uint16_t pid) : vid(vid), pid(pid) {}

}  // namespace policy
