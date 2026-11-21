// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_VARIATIONS_URL_CONSTANTS_H_
#define COMPONENTS_VARIATIONS_VARIATIONS_URL_CONSTANTS_H_

#include "base/component_export.h"

namespace variations {

COMPONENT_EXPORT(VARIATIONS) extern const char kDefaultServerUrl[];

COMPONENT_EXPORT(VARIATIONS) extern const char kDefaultInsecureServerUrl[];

// LINT.IfChange(VariationsSeedKeyIds)
inline constexpr char kSeedKeyIdsHeaderName[] = "X-Seed-Key-Ids";
inline constexpr char kSupportedSeedKeyIdsWithLegacy[] = "1,2,3,0";
inline constexpr char kSupportedSeedKeyIdsWithoutLegacy[] = "1,2,3";
// LINT.ThenChange(//components/variations/android/java/src/org/chromium/components/variations/firstrun/VariationsSeedFetcher.java:VariationsSeedKeyIds)

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_VARIATIONS_URL_CONSTANTS_H_
