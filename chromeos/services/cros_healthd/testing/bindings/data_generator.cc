// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cros_healthd/testing/bindings/data_generator.h"

#include <fcntl.h>

#include "mojo/public/cpp/system/platform_handle.h"

namespace chromeos {
namespace cros_healthd {
namespace connectivity {

constexpr char kDevNull[] = "/dev/null";

::mojo::ScopedHandle HandleDataGenerator::Generate() {
  has_next_ = false;
  return mojo::WrapPlatformFile(
      base::ScopedPlatformFile(open(kDevNull, O_RDONLY)));
}

}  // namespace connectivity
}  // namespace cros_healthd
}  // namespace chromeos
