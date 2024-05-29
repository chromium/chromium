// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/share/android/intent_helper.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/share/android/jni_headers/IntentHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace browser_ui {

void SendEmail(const std::u16string& d_email,
               const std::u16string& d_subject,
               const std::u16string& d_body,
               const std::u16string& d_chooser_title,
               const std::u16string& d_file_to_attach) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_email = ConvertUTF16ToJavaString(env, d_email);
  ScopedJavaLocalRef<jstring> j_subject =
      ConvertUTF16ToJavaString(env, d_subject);
  ScopedJavaLocalRef<jstring> j_body = ConvertUTF16ToJavaString(env, d_body);
  ScopedJavaLocalRef<jstring> j_chooser_title =
      ConvertUTF16ToJavaString(env, d_chooser_title);
  ScopedJavaLocalRef<jstring> j_file_to_attach =
      ConvertUTF16ToJavaString(env, d_file_to_attach);
  Java_IntentHelper_sendEmail(env, j_email, j_subject, j_body, j_chooser_title,
                              j_file_to_attach);
}

}  // namespace browser_ui
