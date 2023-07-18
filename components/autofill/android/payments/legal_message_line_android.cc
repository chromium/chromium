// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/payments/legal_message_line_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/payments_jni_headers/LegalMessageLine_jni.h"

namespace autofill {

/*static*/
base::android::ScopedJavaLocalRef<jobject>
LegalMessageLineAndroid::ConvertToJavaObject(
    const LegalMessageLine& legal_message_line) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_object =
      Java_LegalMessageLine_Constructor(env,
                                        base::android::ConvertUTF16ToJavaString(
                                            env, legal_message_line.text()));
  for (const auto& link : legal_message_line.links()) {
    Java_LegalMessageLine_addLink(
        env, java_object,
        Java_Link_Constructor(
            env, link.range.start(), link.range.end(),
            base::android::ConvertUTF8ToJavaString(env, link.url.spec())));
  }
  return java_object;
}

}  // namespace autofill
