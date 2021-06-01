// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_SWITCHES_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_SWITCHES_H_

namespace variations {
namespace switches {

// Alphabetical list of switches specific to the variations component. Document
// each in the .cc file.

extern const char kDisableFieldTrialTestingConfig[];
extern const char kEnableBenchmarking[];
extern const char kFakeVariationsChannel[];
extern const char kForceFieldTrialParams[];
extern const char kForceVariationIds[];
extern const char kForceDisableVariationIds[];
extern const char kVariationsOverrideCountry[];
extern const char kVariationsServerURL[];
extern const char kVariationsInsecureServerURL[];

}  // namespace switches
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_SWITCHES_H_
