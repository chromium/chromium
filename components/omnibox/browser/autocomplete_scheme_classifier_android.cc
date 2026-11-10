// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_scheme_classifier_android.h"

#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/scheme_classifier_jni/AutocompleteSchemeClassifier_jni.h"

namespace jni_zero {
template <>
AutocompleteSchemeClassifier* FromJniType<AutocompleteSchemeClassifier*>(
    JNIEnv* env,
    const JavaRef<jobject>& obj) {
  if (!obj) {
    return nullptr;
  }
  return Java_AutocompleteSchemeClassifier_getNativePtr(env, obj);
}
}  // namespace jni_zero

DEFINE_JNI(AutocompleteSchemeClassifier)
