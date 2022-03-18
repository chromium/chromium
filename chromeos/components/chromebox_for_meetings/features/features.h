// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_FEATURES_H_
#define CHROMEOS_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace cfm {
namespace features {

// IMPORTANT: Please keep this file in alphabetical order.

// Enables or disables the ability to enqueue cloud telemetry information using
// Chrome Encrypted Reporting Pipeline API.
// Note: Functionality depends on Feature {MeetDevicesMojoServices}
// Note: Enqueue functionality depends on Feature {EncryptedReportingPipeline}
COMPONENT_EXPORT(CFM_FEATURES)
extern const base::Feature kCloudLogger;

// Enables or disables the ability to bind mojo connections through chrome for
// CfM specific mojom based system services.
COMPONENT_EXPORT(CFM_FEATURES)
extern const base::Feature kMojoServices;

}  // namespace features
}  // namespace cfm
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash::cfm::features {
using ::chromeos::cfm::features::kCloudLogger;
using ::chromeos::cfm::features::kMojoServices;
}  // namespace ash::cfm::features

#endif  // CHROMEOS_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_FEATURES_H_
