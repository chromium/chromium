// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_ANDROID_VARIATIONS_SEED_BRIDGE_H_
#define COMPONENTS_VARIATIONS_ANDROID_VARIATIONS_SEED_BRIDGE_H_

#include <jni.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/component_export.h"
#include "components/variations/seed_response.h"

namespace variations {
namespace android {

// Return the first run seed data pulled from the Java side of application.
COMPONENT_EXPORT(VARIATIONS)
std::unique_ptr<variations::SeedResponse> GetVariationsFirstRunSeed();

// Clears first run seed preferences stored on the Java side of Chrome for
// Android.
COMPONENT_EXPORT(VARIATIONS) void ClearJavaFirstRunPrefs();

// Marks variations seed as stored to avoid repeated fetches of the seed at
// the Java side.
COMPONENT_EXPORT(VARIATIONS) void MarkVariationsSeedAsStored();

// Sets test data on the Java side. The data is pulled during the unit tests to
// C++ side and is being checked for consistency.
// This method is used for unit testing purposes only.
COMPONENT_EXPORT(VARIATIONS)
void SetJavaFirstRunPrefsForTesting(const std::string& seed_data,
                                    const std::string& seed_signature,
                                    const std::string& seed_country,
                                    int64_t response_date,
                                    bool is_gzip_compressed);

COMPONENT_EXPORT(VARIATIONS) bool HasMarkedPrefsForTesting();

}  // namespace android
}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_ANDROID_VARIATIONS_SEED_BRIDGE_H_
