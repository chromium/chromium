// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_IMAGE_WRITER_IMAGE_WRITER_UTIL_MAC_H_
#define CHROME_COMMON_EXTENSIONS_IMAGE_WRITER_IMAGE_WRITER_UTIL_MAC_H_

#include <IOKit/IOKitLib.h>

#include <string>

namespace extensions {

// Determines whether the specified disk is suitable for writing an image onto.
// If this function returns true, it also returns other info values; pass
// null if those values are not wanted.
bool IsSuitableRemovableStorageDevice(io_object_t disk_obj,
                                      std::string* out_bsd_name,
                                      uint64_t* out_size_in_bytes,
                                      bool* out_removable);

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_IMAGE_WRITER_IMAGE_WRITER_UTIL_MAC_H_
