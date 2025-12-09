// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_PLATFORM_SUPPORT_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_PLATFORM_SUPPORT_H_

#include <optional>

#include "chromeos/ash/experiences/arc/arc_platform_support.h"

namespace arc {

// A fake implementation of ArcPlatformSupport for testing.
class FakeArcPlatformSupport : public ArcPlatformSupport {
 public:
  FakeArcPlatformSupport() = default;
  FakeArcPlatformSupport(const FakeArcPlatformSupport&) = delete;
  FakeArcPlatformSupport& operator=(const FakeArcPlatformSupport&) = delete;
  ~FakeArcPlatformSupport() override = default;

  // ArcPlatformSupport overrides:
  bool IsDlcEnabled() const override;
  void CheckDlcRequirement() override;

  // Test-specific method to control the state.
  // This must be called before any call to IsDlcEnabled() and the value should
  // not be changed afterward.
  void SetDlcEnabled(bool enabled);

 private:
  std::optional<bool> is_arcvm_dlc_enabled_;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_TEST_FAKE_ARC_PLATFORM_SUPPORT_H_
