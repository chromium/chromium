// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/language/core/common/language_experiments.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/translate/content/android/jni_headers/TranslateFeatureMap_jni.h"

namespace translate::android {
namespace {

// Array of translate features exposed through the Java TranslateFeatureMap
// API. Entries in this array refer to features defined in
// components/language/core/common/language_experiments.h.
const base::Feature* const kFeaturesExposedToJava[] = {
    &language::kContentLanguagesInLanguagePicker,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_TranslateFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace translate::android
