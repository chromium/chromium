// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/omnibox/omnibox_view_util.h"

#include "base/android/jni_string.h"
#include "components/omnibox/browser/omnibox_view.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/OmniboxViewUtil_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jstring> JNI_OmniboxViewUtil_SanitizeTextForPaste(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtext) {
  std::u16string pasted_text(
      base::android::ConvertJavaStringToUTF16(env, jtext));
  pasted_text = OmniboxView::SanitizeTextForPaste(pasted_text);
  return base::android::ConvertUTF16ToJavaString(env, pasted_text);
}
