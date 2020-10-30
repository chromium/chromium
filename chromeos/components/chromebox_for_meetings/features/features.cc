// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/chromebox_for_meetings/features/features.h"

namespace chromeos {
namespace cfm {
namespace features {

// Enables or disables the ability to bind mojo connections through chrome for
// Cfm specific mojom based system services.
const base::Feature kCfmMojoServices{"CfmMojoServices",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the ability to enqueue cloud telemetry information using
// Chrome Encrypted Reporting Pipeline API.
const base::FeatureParam<bool> kCfmTelemetryParam{&kCfmMojoServices,
                                                  "ERPTelemetryService", false};

bool IsCfmMojoEnabled() {
  return base::FeatureList::IsEnabled(
      chromeos::cfm::features::kCfmMojoServices);
}

bool IsCfmTelemetryEnabled() {
  return kCfmTelemetryParam.Get();
}

}  // namespace features
}  // namespace cfm
}  // namespace chromeos
