// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android_auth_client_lib/cpp/byte_array_callback_listener.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "components/ip_protection/android_auth_client_lib/cpp/jni_headers/ByteArrayCallbackListener_jni.h"

namespace ip_protection::android {

base::android::ScopedJavaLocalRef<jobject> ByteArrayCallbackListener::Create(
    base::OnceCallback<void(base::expected<std::string, int>)> callback) {
  return Java_ByteArrayCallbackListener_Constructor(
      base::android::AttachCurrentThread(),
      reinterpret_cast<jlong>(
          new ByteArrayCallbackListener(std::move(callback))));
}

void ByteArrayCallbackListener::OnResult(
    JNIEnv* env,
    jni_zero::JavaParamRef<jbyteArray> response) {
  std::string response_str;
  base::android::JavaByteArrayToString(base::android::AttachCurrentThread(),
                                       response, &response_str);
  std::move(callback_).Run(base::ok(std::move(response_str)));
  delete this;
}

void ByteArrayCallbackListener::OnError(JNIEnv* env, jint errorCode) {
  std::move(callback_).Run(
      base::unexpected(std::move(static_cast<int>(errorCode))));
  delete this;
}

ByteArrayCallbackListener::ByteArrayCallbackListener(
    base::OnceCallback<void(base::expected<std::string, int>)> callback)
    : callback_(std::move(callback)) {}

ByteArrayCallbackListener::~ByteArrayCallbackListener() = default;

}  // namespace ip_protection::android
