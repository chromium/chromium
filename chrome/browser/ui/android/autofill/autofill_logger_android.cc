// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_logger_android.h"

#include "chrome/android/chrome_jni_headers/AutofillLogger_jni.h"

using base::android::ScopedJavaLocalRef;

namespace autofill {

void AutofillLoggerAndroid::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_autofilled_value =
      base::android::ConvertUTF16ToJavaString(env, autofilled_value);
  ScopedJavaLocalRef<jstring> j_profile_full_name =
      base::android::ConvertUTF16ToJavaString(env, profile_full_name);
  // On android, the fields are never previwed: it's safe to assume here that
  // the field has been filled.
  Java_AutofillLogger_didFillField(env, j_autofilled_value,
                                   j_profile_full_name);
}

}  // namespace autofill
