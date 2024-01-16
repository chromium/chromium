// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PREF_NAMES_H_
#define COMPONENTS_VARIATIONS_PREF_NAMES_H_

#include "base/component_export.h"

namespace variations {
namespace prefs {

// Alphabetical list of preference names specific to the variations component.
// Keep alphabetized and document each in the .cc file.

COMPONENT_EXPORT(VARIATIONS)
extern const char kDeviceVariationsRestrictionsByPolicy[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsCompressedSeed[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsCountry[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsCrashStreak[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsFailedToFetchSeedStreak[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsGoogleGroups[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsLastFetchTime[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsLimitedEntropySyntheticTrialSeed[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSeedMilestone[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsPermanentConsistencyCountry[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsPermanentOverriddenCountry[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsRestrictionsByPolicy[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsRestrictParameter[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSafeCompressedSeed[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSafeSeedDate[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSafeSeedFetchTime[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSafeSeedLocale[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSafeSeedMilestone[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsSafeSeedPermanentConsistencyCountry[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsSafeSeedSessionConsistencyCountry[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSafeSeedSignature[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSeedDate[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsSeedSignature[];

// For chrome://field-trial-internals.
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsForcedFieldTrials[];
COMPONENT_EXPORT(VARIATIONS)
extern const char kVariationsForcedTrialExpiration[];
COMPONENT_EXPORT(VARIATIONS) extern const char kVariationsForcedTrialStarts[];

}  // namespace prefs
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PREF_NAMES_H_
