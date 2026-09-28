// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_LEGAL_MESSAGE_LINE_ANDROID_H_
#define COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_LEGAL_MESSAGE_LINE_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/payments_jni_headers/LegalMessageLine_jni.h"

namespace jni_zero {

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::LegalMessageLine::Link>(
    JNIEnv* env,
    const autofill::LegalMessageLine::Link& link) {
  return autofill::Java_Link_Constructor(env, link.range.start(),
                                         link.range.end(), link.url.spec());
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::LegalMessageLine>(
    JNIEnv* env,
    const autofill::LegalMessageLine& legal_message_line) {
  return autofill::Java_LegalMessageLine_Constructor(
      env, legal_message_line.text(), legal_message_line.links());
}

}  // namespace jni_zero

#endif  // COMPONENTS_AUTOFILL_ANDROID_PAYMENTS_LEGAL_MESSAGE_LINE_ANDROID_H_
