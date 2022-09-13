// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_ANDROID_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_ANDROID_H_

#include "base/android/scoped_java_ref.h"

class AutocompleteSchemeClassifier;

class AutocompleteSchemeClassifierAndroid {
 public:
  static AutocompleteSchemeClassifier* FromJavaObj(
      const base::android::JavaParamRef<jobject>&
          jautocomplete_scheme_classifier);

 private:
  AutocompleteSchemeClassifierAndroid() = default;
  ~AutocompleteSchemeClassifierAndroid() = default;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_SCHEME_CLASSIFIER_ANDROID_H_