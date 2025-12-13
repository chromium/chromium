// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/identity_provider_service.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/webid/jni_headers/IdentityProviderService_jni.h"

namespace content::webid {

IdentityProviderService::IdentityProviderService() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_IdentityProviderService_create(
      env, reinterpret_cast<intptr_t>(this)));
}

IdentityProviderService::~IdentityProviderService() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_IdentityProviderService_destroy(env, java_obj_);
  Java_IdentityProviderService_disconnect(env, java_obj_);
}

void IdentityProviderService::Fetch(
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  callback_ = std::move(callback);
  Java_IdentityProviderService_fetch(env, java_obj_);
}

void IdentityProviderService::Connect(const std::string& package_name,
                                      const std::string& service_name,
                                      base::OnceCallback<void(bool)> callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  connect_callback_ = std::move(callback);
  Java_IdentityProviderService_connect(env, java_obj_, package_name,
                                       service_name);
}

void IdentityProviderService::Disconnect(base::OnceCallback<void()> callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  disconnect_callback_ = std::move(callback);
  Java_IdentityProviderService_disconnect(env, java_obj_);
}

void IdentityProviderService::OnDataFetched(JNIEnv* env,
                                            std::optional<std::string> data) {
  std::move(callback_).Run(data);
}

void IdentityProviderService::OnConnected(JNIEnv* env, bool success) {
  std::move(connect_callback_).Run(success);
}

void IdentityProviderService::OnDisconnected(JNIEnv* env) {
  std::move(disconnect_callback_).Run();
}

DEFINE_JNI(IdentityProviderService)

}  // namespace content::webid
