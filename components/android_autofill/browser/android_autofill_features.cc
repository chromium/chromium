// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/android_autofill/browser/android_autofill_features.h"

#include <jni.h>

#include "base/feature_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/android_autofill/browser/jni_headers_features/AndroidAutofillFeatures_jni.h"

namespace autofill::features {

namespace {

const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidAutofillBottomSheetWorkaround,
    &kAndroidAutofillPrefillRequestsForLoginForms,
};

}  // namespace

// If enabled, we send SparseArrayWithWorkaround class as the PrefillHints for
// the platform API `AutofillManager.notifyViewReady()` as a workaround for the
// platform bug, see the comment on the class. This works as a kill switch for
// the workaround in case any unexpected thing goes wrong.
BASE_FEATURE(kAndroidAutofillBottomSheetWorkaround,
             "AndroidAutofillBottomSheetWorkaround",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we explicitly cancel the ongoing Android autofill session on new
// document navigation by calling `AutofillManager.cancel()`, we clear the
// request state in the java side as it works as an indicator to the current
// session.
BASE_FEATURE(kAndroidAutofillCancelSessionOnNavigation,
             "AndroidAutofillCancelSessionOnNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, we stop relying on `known_success` in FormSubmitted signal to
// decide whether to defer submission on not, and instead we directly inform the
// provider of submission.
BASE_FEATURE(kAndroidAutofillDirectFormSubmission,
             "AndroidAutofillDirectFormSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, prefill requests (i.e. calls to
// `AutofillManager.notifyVirtualViewsReady`) are supported. Such prefill
// requests are sent at most once per WebView session and are limited to forms
// that are assumed to be login forms.
// Future features may extend prefill requests to more form types.
BASE_FEATURE(kAndroidAutofillPrefillRequestsForLoginForms,
             "AndroidAutofillPrefillRequestsForLoginForms",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, offer prefill requests (i.e. calls to
// `AutofillManager.notifyVirtualViewsReady`) to change
// password forms as well. A form can't be login and change password at the same
// time so order of the check whether it's login or change password shouldn't
// matter.
BASE_FEATURE(kAndroidAutofillPrefillRequestsForChangePassword,
             "AndroidAutofillPrefillRequestsForChangePassword",
             base::FEATURE_DISABLED_BY_DEFAULT);

static jlong JNI_AndroidAutofillFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace autofill::features
