// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_BYTE_ARRAY_CALLBACK_LISTENER_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_BYTE_ARRAY_CALLBACK_LISTENER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"

namespace ip_protection::android {

// Native ByteArrayCallbackListener receives a call from the Java
// ByteArrayCallbackListener through JNI using OnResult or OnError, and calls
// its callback. After calling its callback, this class self-destructs.
class ByteArrayCallbackListener {
 public:
  static base::android::ScopedJavaLocalRef<jobject> Create(
      base::OnceCallback<void(base::expected<std::string, AuthRequestError>)>
          callback);

  // Called by Java.
  void OnResult(JNIEnv* env, jni_zero::JavaParamRef<jbyteArray> response);

  // Called by Java.
  void OnError(JNIEnv* env, jint authRequestError);

 private:
  explicit ByteArrayCallbackListener(
      base::OnceCallback<void(base::expected<std::string, AuthRequestError>)>
          callback);
  ~ByteArrayCallbackListener();

  base::OnceCallback<void(base::expected<std::string, AuthRequestError>)>
      callback_;
};

}  // namespace ip_protection::android

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_BYTE_ARRAY_CALLBACK_LISTENER_H_
