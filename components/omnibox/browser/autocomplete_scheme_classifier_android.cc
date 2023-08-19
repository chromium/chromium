// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scheme_classifier_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/scheme_classifier_jni/AutocompleteSchemeClassifier_jni.h"

// static
AutocompleteSchemeClassifier* AutocompleteSchemeClassifierAndroid::FromJavaObj(
    const base::android::JavaParamRef<jobject>&
        jautocomplete_scheme_classifier) {
  return reinterpret_cast<AutocompleteSchemeClassifier*>(
      Java_AutocompleteSchemeClassifier_getNativePtr(
          base::android::AttachCurrentThread(),
          jautocomplete_scheme_classifier));
}
