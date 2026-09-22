// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/payments/legal_message_line_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"

// Must come after headers that provide symbols used by @JniType.
#include "components/autofill/android/payments_jni_headers/LegalMessageLine_jni.h"

namespace autofill {

using jni_zero::ScopedJavaLocalRef;

// static
ScopedJavaLocalRef<jobject> LegalMessageLineAndroid::ConvertToJavaObject(
    const LegalMessageLine& legal_message_line) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_object =
      Java_LegalMessageLine_Constructor(env, legal_message_line.text());
  for (const auto& link : legal_message_line.links()) {
    Java_LegalMessageLine_addLink(env, java_object, link.range.start(),
                                  link.range.end(), link.url.spec());
  }
  return java_object;
}

// static
std::vector<ScopedJavaLocalRef<jobject>>
LegalMessageLineAndroid::ConvertToJavaLinkedList(
    const std::vector<LegalMessageLine>& legal_message_lines) {
  return base::ToVector(legal_message_lines,
                        &LegalMessageLineAndroid::ConvertToJavaObject);
}

}  // namespace autofill

DEFINE_JNI(LegalMessageLine)
