// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_FEATURES_H_
#define CHROMEOS_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_FEATURES_H_

#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"

namespace chromeos {
namespace cfm {
namespace features {

COMPONENT_EXPORT(CFM_FEATURES)
extern const base::Feature kCfmMojoServices;

COMPONENT_EXPORT(CFM_FEATURES)
extern const base::FeatureParam<bool> kCfmTelemetryParam;

// Whether cross platform mojo connections is enabled.
COMPONENT_EXPORT(CFM_FEATURES)
bool IsCfmMojoEnabled();

// Whether Telemetry through Encrypted Reporting Pipeline is enabled.
COMPONENT_EXPORT(CFM_FEATURES)
bool IsCfmTelemetryEnabled();

}  // namespace features
}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CHROMEBOX_FOR_MEETINGS_FEATURES_FEATURES_H_
