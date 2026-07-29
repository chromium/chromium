// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_DISAMBIGUATOR_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_DISAMBIGUATOR_H_

#include <string>
#include <vector>

namespace syncer {

class DeviceInfo;

// Returns a list of display names for the given devices when
// `kSyncSimplifyDeviceNaming` is enabled. Disambiguates duplicate display names
// across devices and against `local_device` by using release channel labels.
// TODO(crbug.com/522788942): Consolidate this with device_name_util.h such that
// there is a single way to get device names.
std::vector<std::string> GetDeviceDisplayNamesListDisambiguatedByChannel(
    const std::vector<const DeviceInfo*>& devices,
    const DeviceInfo* local_device);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_NAME_DISAMBIGUATOR_H_
