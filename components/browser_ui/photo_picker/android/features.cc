// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/browser_ui/photo_picker/android/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/photo_picker/android/photo_picker_jni_headers/PhotoPickerFeatures_jni.h"

namespace photo_picker {
namespace features {

namespace {

// Array of features exposed through the Java Features bridge class. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h), and must be
// replicated in the same order in PhotoPickerFeatures.java.
const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidMediaPickerAdoption,
};

}  // namespace

BASE_FEATURE(kAndroidMediaPickerAdoption,
             "MediaPickerAdoption",
             base::FEATURE_DISABLED_BY_DEFAULT);

static jlong JNI_PhotoPickerFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace features
}  // namespace photo_picker
