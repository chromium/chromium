// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_PREF_NAMES_H_
#define COMPONENTS_VARIATIONS_PREF_NAMES_H_

namespace variations {
namespace prefs {

// Alphabetical list of preference names specific to the variations component.
// Keep alphabetized, and document each in the .cc file.

extern const char kVariationsCompressedSeed[];
extern const char kVariationsCountry[];
extern const char kVariationsCrashStreak[];
extern const char kVariationsFailedToFetchSeedStreak[];
extern const char kVariationsLastFetchTime[];
extern const char kVariationsPermanentConsistencyCountry[];
extern const char kVariationsPermanentOverriddenCountry[];
extern const char kVariationsRestrictParameter[];
extern const char kVariationsSafeCompressedSeed[];
extern const char kVariationsSafeSeedDate[];
extern const char kVariationsSafeSeedFetchTime[];
extern const char kVariationsSafeSeedLocale[];
extern const char kVariationsSafeSeedPermanentConsistencyCountry[];
extern const char kVariationsSafeSeedSessionConsistencyCountry[];
extern const char kVariationsSafeSeedSignature[];
extern const char kVariationsSeedDate[];
extern const char kVariationsSeedSignature[];

}  // namespace prefs
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_PREF_NAMES_H_
