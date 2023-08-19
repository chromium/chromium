// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/payments/legal_message_line_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/payments_jni_headers/LegalMessageLine_jni.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

namespace autofill {

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jobject> LegalMessageLineAndroid::ConvertToJavaObject(
    const LegalMessageLine& legal_message_line) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_object = Java_LegalMessageLine_Constructor(
      env, ConvertUTF16ToJavaString(env, legal_message_line.text()));
  for (const auto& link : legal_message_line.links()) {
    Java_LegalMessageLine_addLink(
        env, java_object,
        Java_Link_Constructor(env, link.range.start(), link.range.end(),
                              ConvertUTF8ToJavaString(env, link.url.spec())));
  }
  return java_object;
}

// static
ScopedJavaLocalRef<jobject> LegalMessageLineAndroid::ConvertToJavaLinkedList(
    const std::vector<LegalMessageLine>& legal_message_lines) {
  // Initially null, will be initialized in
  // `Java_LegalMessageLine_addToList_createListIfNull()` below.
  ScopedJavaLocalRef<jobject> linked_list;

  JNIEnv* env = base::android::AttachCurrentThread();
  for (const auto& line : legal_message_lines) {
    // Creates a new LinkedList in Java when the passed in `linked_list`
    // parameter is null.
    linked_list = Java_LegalMessageLine_addToList_createListIfNull(
        env, linked_list, ConvertUTF16ToJavaString(env, line.text()));
    for (const auto& link : line.links()) {
      Java_LegalMessageLine_addLinkToLastInList(
          env, linked_list, link.range.start(), link.range.end(),
          ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }
  return linked_list;
}

}  // namespace autofill
