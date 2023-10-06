// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SWITCHES_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SWITCHES_H_

#include "base/component_export.h"

namespace variations {
namespace switches {

// Alphabetical list of switches specific to the variations component. Document
// each in the .cc file.

COMPONENT_EXPORT(VARIATIONS)
extern const char kDisableFieldTrialTestingConfig[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kDisableVariationsSafeMode[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kDisableVariationsSeedFetchThrottling[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kEnableBenchmarking[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kEnableFieldTrialTestingConfig[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kFakeVariationsChannel[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kForceFieldTrialParams[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kForceVariationIds[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kForceDisableVariationIds[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsSeedVersion[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsOverrideCountry[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsTestSeedJsonPath[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsServerURL[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsInsecureServerURL[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsSeedFetchInterval[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kEnableFinchSeedDeltaCompression[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kAcceptEmptySeedSignatureForTesting[];

}  // namespace switches
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SWITCHES_H_
