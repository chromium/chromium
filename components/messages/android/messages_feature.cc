// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/messages_feature.h"

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/messages/android/feature_flags_jni_headers/MessageFeatureMap_jni.h"

namespace messages {

namespace {

const base::Feature* const kFeaturesExposedToJava[] = {
    &kMessagesForAndroidFullyVisibleCallback, &kMessagesAndroidExtraHistograms,
    &kMessagesCloseButton};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

BASE_FEATURE(kMessagesForAndroidFullyVisibleCallback,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature that enables extra histogram recordings.
BASE_FEATURE(kMessagesAndroidExtraHistograms, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesCloseButton, base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_MessageFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace messages

DEFINE_JNI(MessageFeatureMap)
