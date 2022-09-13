// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STORAGE_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_
#define COMPONENTS_STORAGE_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace storage_monitor {

// Prefix constants used in device unique id.
extern const char kFSUniqueIdPrefix[];
extern const char kVendorModelSerialPrefix[];

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
extern const char kVendorModelVolumeStoragePrefix[];
#endif

#if BUILDFLAG(IS_WIN)
// Windows portable device interface GUID constant.
extern const wchar_t kWPDDevInterfaceGUID[];
#endif

extern const base::FilePath::CharType kDCIMDirectoryName[];

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_REMOVABLE_DEVICE_CONSTANTS_H_
