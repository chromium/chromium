// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/storage_monitor/removable_device_constants.h"

namespace storage_monitor {

const char kFSUniqueIdPrefix[] = "UUID:";
const char kVendorModelSerialPrefix[] = "VendorModelSerial:";

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const char kVendorModelVolumeStoragePrefix[] = "VendorModelVolumeStorage:";
#endif

#if BUILDFLAG(IS_WIN)
const wchar_t kWPDDevInterfaceGUID[] =
    L"{6ac27878-a6fa-4155-ba85-f98f491d4f33}";
#endif

const base::FilePath::CharType kDCIMDirectoryName[] = FILE_PATH_LITERAL("DCIM");

}  // namespace storage_monitor
