// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cros_healthd/testing/bindings/data_generator.h"

#include <fcntl.h>

#include "mojo/public/cpp/system/platform_handle.h"

namespace ash::cros_healthd::connectivity {

constexpr char kDevNull[] = "/dev/null";

::mojo::ScopedHandle HandleDataGenerator::Generate() {
  has_next_ = false;
  return mojo::WrapPlatformFile(
      base::ScopedPlatformFile(open(kDevNull, O_RDONLY)));
}

}  // namespace ash::cros_healthd::connectivity
