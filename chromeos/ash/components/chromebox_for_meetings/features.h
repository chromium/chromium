// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_H_
#define CHROMEOS_ASH_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace ash::cfm::features {

// IMPORTANT: Please keep this file in alphabetical order.

// Enables or disables the ability to enqueue cloud telemetry information using
// Artemis. Note that Artemis != the cloud logger that is running in hotrod.
// Note: Functionality depends on Feature {MeetDevicesMojoServices}
// Note: Enqueue functionality depends on Feature {EncryptedReportingPipeline}
COMPONENT_EXPORT(CFM_FEATURES) BASE_DECLARE_FEATURE(kCloudLogger);

// Enables or disables the ability to bind mojo connections through chrome for
// CfM specific mojom based system services.
COMPONENT_EXPORT(CFM_FEATURES) BASE_DECLARE_FEATURE(kMojoServices);

// Enables or disables the ability to use Meet XU controls.
COMPONENT_EXPORT(CFM_FEATURES) BASE_DECLARE_FEATURE(kXuControls);

}  // namespace ash::cfm::features

#endif  // CHROMEOS_ASH_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_H_
