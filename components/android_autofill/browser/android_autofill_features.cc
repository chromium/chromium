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
    &kAndroidAutofillLazyFrameworkWrapper,
    &kAutofillVirtualViewStructureAndroidPasskeyLongPress,
    &kAndroidAutofillForwardIframeOrigin,
    &kAndroidAutofillUpdateContextForWebContents,
    &kAndroidAutofillImprovedVisibilityDetection};

}  // namespace

// If enabled, at least one passkey must be present to forward passkey requests
// to the Android Credential Manager. Users can then always (re-)trigger the
// passkey request with a long-press action on webauthn-annotated fields.
BASE_FEATURE(kAutofillVirtualViewStructureAndroidPasskeyLongPress,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the AutofillManagerWrapper class will not be initialized when the
// AutofillProvider Java class is initialized. Some apps do not use Autofill at
// all, yet they incur the latency cost of initializing the wrapper. This
// experiment tests whether lazily initializing the wrapper will cause any
// issues.
BASE_FEATURE(kAndroidAutofillLazyFrameworkWrapper,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the origin of a field is forwarded to the Autofill framework if
// it differs from the origin of the main frame.
BASE_FEATURE(kAndroidAutofillForwardIframeOrigin,
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, an additional custom "visible" attribute in each node's HtmlInfo
// is set and sent to the framework.
BASE_FEATURE(kAndroidAutofillImprovedVisibilityDetection,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the native autofill provider is updated when the web contents
// change.
BASE_FEATURE(kAndroidAutofillUpdateContextForWebContents,
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, the URL of the current page is passed to the HttpAuth dialog for
// autofill purposes. Functions as a kill switch. Remove in or after M146.
BASE_FEATURE(kAndroidAutofillSupportForHttpAuth,
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_AndroidAutofillFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace autofill::features

DEFINE_JNI(AndroidAutofillFeatures)
