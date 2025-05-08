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

const base::Feature* const kFeaturesExposedToJava[] = {
    &kAndroidAutofillDeprecateAccessibilityApi,
    &kAutofillVirtualViewStructureAndroidInCct,
    &kAndroidAutofillLazyFrameworkWrapper,
    &kAutofillVirtualViewStructureAndroidPasskeyLongPress};

}  // namespace

// If enabled, autofill calls are never falling back to the accessibility APIs.
// This feature is meant to be enabled after AutofillVirtualViewStructureAndroid
// which provides alternative paths to handle autofill requests.
BASE_FEATURE(kAndroidAutofillDeprecateAccessibilityApi,
             "AndroidAutofillDeprecateAccessibilityApi",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Safe-guard for a crucial fix that prevented consistent use of 3P in CCTs.
// It's ineffective when AutofillVirtualViewStructureAndroid is disabled.
// TODO: crbug.com/409579377 - Delete after M140.
BASE_FEATURE(kAutofillVirtualViewStructureAndroidInCct,
             "AutofillVirtualViewStructureAndroidInCct",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, at least one passkey must be present to forward passkey requests
// to the Android Credential Manager. Users can then always (re-)trigger the
// passkey request with a long-press action on webauthn-annotated fields.
BASE_FEATURE(kAutofillVirtualViewStructureAndroidPasskeyLongPress,
             "AutofillVirtualViewStructureAndroidPasskeyLongPress",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the AutofillManagerWrapper class will not be initialized when the
// AutofillProvider Java class is initialized. Some apps do not use Autofill at
// all, yet they incur the latency cost of initializing the wrapper. This
// experiment tests whether lazily initializing the wrapper will cause any
// issues.
BASE_FEATURE(kAndroidAutofillLazyFrameworkWrapper,
             "AndroidAutofillLazyFrameworkWrapper",
             base::FEATURE_DISABLED_BY_DEFAULT);

static jlong JNI_AndroidAutofillFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace autofill::features
