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
    &kAndroidAutofillBottomSheetWorkaround,
    &kAndroidAutofillFormSubmissionCheckById,
    &kAndroidAutofillPrefillRequestsForLoginForms,
    &kAndroidAutofillSupportVisibilityChanges,
};

}  // namespace

// If enabled, we send SparseArrayWithWorkaround class as the PrefillHints for
// the platform API `AutofillManager.notifyViewReady()` as a workaround for the
// platform bug, see the comment on the class. This works as a kill switch for
// the workaround in case any unexpected thing goes wrong.
BASE_FEATURE(kAndroidAutofillBottomSheetWorkaround,
             "AndroidAutofillBottomSheetWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, form submissions are reported to Android Autofill iff the
// `FormGlobalId` of the submitted form matches that of the current Autofill
// session.
// If disabled, a similarity check is used that requires most (see
// `FormDataAndroid::SimilarAs` for details) members variables of the forms and
// their fields to be identical.
BASE_FEATURE(kAndroidAutofillFormSubmissionCheckById,
             "AndroidAutofillFormSubmissionCheckById",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, prefill requests (i.e. calls to
// `AutofillManager.notifyVirtualViewsReady`) are supported. Such prefill
// requests are sent at most once per WebView session and are limited to forms
// that are assumed to be login forms.
// Future features may extend prefill requests to more form types.
BASE_FEATURE(kAndroidAutofillPrefillRequestsForLoginForms,
             "AndroidAutofillPrefillRequestsForLoginForms",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, visibility changes of form fields of the form of an ongoing
// Autofill session are communicated to Android's `AutofillManager` by calling
// `AutofillManager.notifyViewVisibilityChanged()`.
// See
// https://developer.android.com/reference/android/view/autofill/AutofillManager#notifyViewVisibilityChanged(android.view.View,%20int,%20boolean)
// for more details on the API.
BASE_FEATURE(kAndroidAutofillSupportVisibilityChanges,
             "AndroidAutofillSupportVisibilityChanges",
             base::FEATURE_DISABLED_BY_DEFAULT);

static jlong JNI_AndroidAutofillFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace autofill::features
