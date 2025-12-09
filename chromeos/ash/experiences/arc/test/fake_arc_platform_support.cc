// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/test/fake_arc_platform_support.h"

#include "base/check.h"

namespace arc {

bool FakeArcPlatformSupport::IsDlcEnabled() const {
  CHECK(is_arcvm_dlc_enabled_.has_value());
  return is_arcvm_dlc_enabled_.value();
}

void FakeArcPlatformSupport::CheckDlcRequirement() {
  // In the fake, this is a no-op. The enabled state is controlled
  // directly by calling set_dlc_enabled().
}

void FakeArcPlatformSupport::SetDlcEnabled(bool enabled) {
  CHECK(!is_arcvm_dlc_enabled_.has_value());
  is_arcvm_dlc_enabled_ = enabled;
}

}  // namespace arc
