// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>
#include <string>

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "components/search_engines/search_engines_switches.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/search_engines/android/features_jni_headers/SearchEnginesFeatureMap_jni.h"

namespace search_engines {

namespace {

// Array of features exposed through the Java BaseFeatureMap API. Entries in
// this array refer to features defined in //search_engines features.
const base::Feature* const kFeaturesExposedToJava[] = {
    &switches::kClayBlocking};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_SearchEnginesFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace search_engines
