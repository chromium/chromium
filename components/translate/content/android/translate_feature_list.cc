// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/language/core/common/language_experiments.h"
#include "components/translate/content/android/jni_headers/TranslateFeatureList_jni.h"

namespace translate {
namespace android {
namespace {

// Array of translate features exposed through the Java TranslateFeatureList
// API. Entries in this array refer to features defined in
// components/language/core/common/language_experiments.h.
const base::Feature* kFeaturesExposedToJava[] = {
    &language::kContentLanguagesInLanguagePicker,
    &language::kDetectedSourceLanguageOption,
};

// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList
// exists.
const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (size_t i = 0; i < base::size(kFeaturesExposedToJava); ++i) {
    if (kFeaturesExposedToJava[i]->name == feature_name)
      return kFeaturesExposedToJava[i];
  }
  NOTREACHED() << "Queried feature cannot be found in TranslateFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

static jboolean JNI_TranslateFeatureList_IsEnabled(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature = FindFeatureExposedToJava(
      base::android::ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace android
}  // namespace translate
