// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/common_utils.h"

#include "base/logging.h"
#include "ui/ozone/public/ozone_platform.h"

namespace exo {

bool IsDrmAtomicAvailable() {
  auto& host_properties =
      ui::OzonePlatform::GetInstance()->GetPlatformRuntimeProperties();
  return host_properties.supports_overlays;
}

}  // namespace exo
