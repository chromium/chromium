// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/secure_payload_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/facilitated_payments/android/java/jni_headers/SecureData_jni.h"
#include "components/facilitated_payments/android/java/jni_headers/SecurePayload_jni.h"

namespace payments::facilitated {

namespace {

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

}  // namespace

ScopedJavaLocalRef<jobject> ConvertSecurePayloadToJavaObject(
    const SecurePayload& secure_payload) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> action_token =
      base::android::ToJavaByteArray(env, secure_payload.action_token);
  std::vector<base::android::ScopedJavaLocalRef<jobject>> secure_data_array;
  for (const SecureData& secure_data : secure_payload.secure_data) {
    secure_data_array.push_back(Java_SecureData_Constructor(
        env, secure_data.key, ConvertUTF8ToJavaString(env, secure_data.value)));
  }
  return Java_SecurePayload_create(env, action_token,
                                   std::move(secure_data_array));
}

}  // namespace payments::facilitated

DEFINE_JNI(SecureData)
DEFINE_JNI(SecurePayload)
