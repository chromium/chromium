// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/messages_feature.h"

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/messages/android/feature_flags_jni_headers/MessageFeatureMap_jni.h"

namespace messages {

namespace {

const base::Feature* kFeaturesExposedToJava[] = {
    &kMessagesForAndroidFullyVisibleCallback,
    &kMessagesAndroidExtraHistograms,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace


BASE_FEATURE(kMessagesForAndroidFullyVisibleCallback,
             "MessagesForAndroidFullyVisibleCallback",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature that enables extra histogram recordings.
BASE_FEATURE(kMessagesAndroidExtraHistograms,
             "MessagesAndroidExtraHistograms",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_MessageFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace messages
