// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/payments/legal_message_line_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/payments_jni_headers/LegalMessageLine_jni.h"

namespace autofill {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jobject> LegalMessageLineAndroid::ConvertToJavaObject(
    const LegalMessageLine& legal_message_line) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_object =
      Java_LegalMessageLine_Constructor(env, legal_message_line.text());
  for (const auto& link : legal_message_line.links()) {
    Java_LegalMessageLine_addLink(
        env, java_object,
        Java_Link_Constructor(env, link.range.start(), link.range.end(),
                              ConvertUTF8ToJavaString(env, link.url.spec())));
  }
  return java_object;
}

// static
std::vector<ScopedJavaLocalRef<jobject>>
LegalMessageLineAndroid::ConvertToJavaLinkedList(
    const std::vector<LegalMessageLine>& legal_message_lines) {
  std::vector<ScopedJavaLocalRef<jobject>> list;

  JNIEnv* env = base::android::AttachCurrentThread();
  for (const auto& line : legal_message_lines) {
    list.emplace_back(Java_LegalMessageLine_Constructor(env, line.text()));
    for (const auto& link : line.links()) {
      Java_LegalMessageLine_addLink(env, list.back(), link.range.start(),
                                    link.range.end(), link.url.spec());
    }
  }
  return list;
}

}  // namespace autofill
