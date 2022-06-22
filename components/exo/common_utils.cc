// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/common_utils.h"

#include "base/logging.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace exo {

bool IsDrmAtomicAvailable() {
#if defined(USE_OZONE)
  auto& host_properties =
      ui::OzonePlatform::GetInstance()->GetPlatformRuntimeProperties();
  return host_properties.supports_overlays;
#else
  LOG(WARNING) << "Ozone disabled, cannot determine whether DrmAtomic is "
                  "present. Assuming it is not";
  return false;
#endif
}

}  // namespace exo
