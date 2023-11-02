// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/photo_picker/android/features.h"

#include "components/browser_ui/photo_picker/android/photo_picker_jni_headers/PhotoPickerFeatures_jni.h"

namespace photo_picker {
namespace features {

namespace {

// Array of features exposed through the Java Features bridge class. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h), and must be
// replicated in the same order in PhotoPickerFeatures.java.
const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidMediaPickerSupport,
    &kPhotoPickerVideoSupport,
};

}  // namespace

BASE_FEATURE(kAndroidMediaPickerSupport,
             "AndroidMediaPickerSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPhotoPickerVideoSupport,
             "PhotoPickerVideoSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_PhotoPickerFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace features
}  // namespace photo_picker
