// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_UTIL_H_

#include <string>

#include "components/sync/protocol/sync_enums.pb.h"

namespace syncer {

sync_pb::SyncEnums::DeviceType GetLocalDeviceType();

#if defined(OS_CHROMEOS)
std::string GetChromeOSDeviceNameFromType();
#endif

// Returns the personalizable device name. This may contain
// personally-identifiable information - e.g. Alex's MacbookPro.
std::string GetPersonalizableDeviceNameBlocking();

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_LOCAL_DEVICE_INFO_UTIL_H_
