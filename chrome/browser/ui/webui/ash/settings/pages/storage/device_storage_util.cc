// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/storage/device_storage_util.h"

#include <bit>

namespace ash::settings {

int64_t RoundByteSize(int64_t bytes) {
  if (bytes < 0) {
    return -1;
  }

  if (bytes == 0) {
    return 0;
  }

  return std::bit_ceil(static_cast<uint64_t>(bytes));
}

}  // namespace ash::settings
