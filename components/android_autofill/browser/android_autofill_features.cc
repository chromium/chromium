// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_features.h"

#include <jni.h>

#include "base/feature_list.h"
#include "components/android_autofill/browser/jni_headers_features/AndroidAutofillFeatures_jni.h"

namespace autofill::features {

namespace {

const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidAutofillFormSubmissionCheckById,
    &kAndroidAutofillViewStructureWithFormHierarchyLayer,
};

}  // namespace

// If enabled, form submissions are reported to Android Autofill iff the
// `FormGlobalId` of the submitted form matches that of the current Autofill
// session.
// If disabled, a similarity check is used that requires most (see
// `FormDataAndroid::SimilarAs` for details) members variables of the forms and
// their fields to be identical.
BASE_FEATURE(kAndroidAutofillFormSubmissionCheckById,
             "AndroidAutofillFormSubmissionCheckById",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Adds an additional hierarchy layer for forms into the `ViewStructure` that
// is passed to Android's `AutofillManager`.
// If the feature is disabled, AutofillProvider.java returns a `ViewStructure`
// of depth 1: All form field elements are represented as child nodes of the
// filled `ViewStructure`.
// If the feature is enabled, there is an additional hierarchy level:
// * The child nodes of the filled `ViewStructure` correspond to forms.
// * The child nodes of nodes representing forms correspond to form field
//   elements of the respective form.
BASE_FEATURE(kAndroidAutofillViewStructureWithFormHierarchyLayer,
             "AndroidAutofillViewStructureWithFormHierarchyLayer",
             base::FEATURE_DISABLED_BY_DEFAULT);

static jlong JNI_AndroidAutofillFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace autofill::features
