// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android/android_auth_client_lib/cpp/bind_callback_listener.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/jni_headers/BindCallbackListener_jni.h"

namespace ip_protection::android {

base::android::ScopedJavaLocalRef<jobject> BindCallbackListener::Create(
    base::OnceCallback<IpProtectionAuthClientInterface::ClientCreated>
        callback) {
  return Java_BindCallbackListener_Constructor(
      base::android::AttachCurrentThread(),
      reinterpret_cast<jlong>(new BindCallbackListener(std::move(callback))));
}

void BindCallbackListener::OnResult(JNIEnv* env,
                                    jni_zero::JavaParamRef<jobject> client) {
  std::move(callback_).Run(
      base::WrapUnique(new IpProtectionAuthClient(client)));
  delete this;
}

void BindCallbackListener::OnError(JNIEnv* env, std::string error) {
  std::move(callback_).Run(base::unexpected(std::move(error)));
  delete this;
}

BindCallbackListener::BindCallbackListener(
    base::OnceCallback<IpProtectionAuthClientInterface::ClientCreated> callback)
    : callback_(std::move(callback)) {}

BindCallbackListener::~BindCallbackListener() = default;

}  // namespace ip_protection::android
