// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_ANDROID_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_ANDROID_H_

#include <jni.h>

#include "third_party/jni_zero/jni_zero.h"

class AutocompleteSchemeClassifier;

namespace jni_zero {
template <>
AutocompleteSchemeClassifier* FromJniType<AutocompleteSchemeClassifier*>(
    JNIEnv* env,
    const JavaRef<jobject>& obj);
}  // namespace jni_zero

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_ANDROID_H_
