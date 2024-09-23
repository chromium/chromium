// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_BIND_CALLBACK_LISTENER_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_BIND_CALLBACK_LISTENER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"

namespace ip_protection::android {

// Native BindCallbackListener receives a call from the Java
// BindCallbackListener through JNI using OnResult or OnError, and calls
// its callback. After calling its callback, this class self-destructs.
class BindCallbackListener {
 public:
  static base::android::ScopedJavaLocalRef<jobject> Create(
      base::OnceCallback<IpProtectionAuthClientInterface::ClientCreated>
          callback);

  // Called by Java.
  void OnResult(JNIEnv* env, jni_zero::JavaParamRef<jobject> client);

  // Called by Java.
  void OnError(JNIEnv* env, std::string error);

 private:
  explicit BindCallbackListener(
      base::OnceCallback<IpProtectionAuthClientInterface::ClientCreated>
          callback);
  ~BindCallbackListener();
  base::OnceCallback<IpProtectionAuthClientInterface::ClientCreated> callback_;
};

}  // namespace ip_protection::android

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_BIND_CALLBACK_LISTENER_H_
