// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/scheme_classifier_jni/OmniboxUrlEmphasizer_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jintArray>
JNI_OmniboxUrlEmphasizer_ParseForEmphasizeComponents(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtext,
    const base::android::JavaParamRef<jobject>&
        jautocomplete_scheme_classifier) {
  AutocompleteSchemeClassifier* autocomplete_scheme_classifier =
      AutocompleteSchemeClassifierAndroid::FromJavaObj(
          jautocomplete_scheme_classifier);
  DCHECK(autocomplete_scheme_classifier);

  std::u16string text(base::android::ConvertJavaStringToUTF16(env, jtext));

  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text, *autocomplete_scheme_classifier, &scheme, &host);

  int emphasize_values[] = {scheme.begin, scheme.len, host.begin, host.len};
  return base::android::ToJavaIntArray(env, emphasize_values);
}
