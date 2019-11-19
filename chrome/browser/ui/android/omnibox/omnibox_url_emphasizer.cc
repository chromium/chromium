// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/omnibox/omnibox_url_emphasizer.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/OmniboxUrlEmphasizer_jni.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/omnibox/browser/autocomplete_input.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jintArray>
JNI_OmniboxUrlEmphasizer_ParseForEmphasizeComponents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jstring>& jtext) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  DCHECK(profile);

  base::string16 text(base::android::ConvertJavaStringToUTF16(env, jtext));

  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text, ChromeAutocompleteSchemeClassifier(profile), &scheme, &host);

  int emphasize_values[] = {scheme.begin, scheme.len, host.begin, host.len};
  return base::android::ToJavaIntArray(env, emphasize_values, 4);
}
