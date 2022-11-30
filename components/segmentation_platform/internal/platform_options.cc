// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/platform_options.h"

#include "base/command_line.h"
#include "components/segmentation_platform/internal/constants.h"

namespace segmentation_platform {

PlatformOptions::PlatformOptions(bool force_refresh_results)
    : force_refresh_results(force_refresh_results) {}

// static
PlatformOptions PlatformOptions::CreateDefault() {
  return PlatformOptions(base::CommandLine::ForCurrentProcess()->HasSwitch(
      kSegmentationPlatformRefreshResultsSwitch));
}

}  // namespace segmentation_platform
