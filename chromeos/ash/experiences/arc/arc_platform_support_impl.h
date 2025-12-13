// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PLATFORM_SUPPORT_IMPL_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PLATFORM_SUPPORT_IMPL_H_

#include <optional>

#include "chromeos/ash/experiences/arc/arc_platform_support.h"

namespace arc {

// The production implementation of ArcPlatformSupport.
class ArcPlatformSupportImpl : public ArcPlatformSupport {
 public:
  ArcPlatformSupportImpl() = default;
  ArcPlatformSupportImpl(const ArcPlatformSupportImpl&) = delete;
  ArcPlatformSupportImpl& operator=(const ArcPlatformSupportImpl&) = delete;
  ~ArcPlatformSupportImpl() override = default;

  // ArcPlatformSupport overrides:
  bool IsDlcEnabled() const override;
  void CheckDlcRequirement() override;

 private:
  std::optional<bool> is_arcvm_dlc_enabled_;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_ARC_PLATFORM_SUPPORT_IMPL_H_
