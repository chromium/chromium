// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_accessibility_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillAccessibilityUtils_jni.h"

using base::android::ScopedJavaLocalRef;

namespace autofill {

void AnnounceTextForA11y(const std::u16string& message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillAccessibilityUtils_announce(
      env, base::android::ConvertUTF16ToJavaString(env, message));
}

}  // namespace autofill
