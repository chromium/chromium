// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/identity_provider_service.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
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
    const std::string& url,
    const std::optional<std::string>& body,
    const base::flat_map<std::string, std::string>& headers,
    base::OnceCallback<void(const std::optional<std::string>&)> callback) {
  DCHECK(!callback_);
  JNIEnv* env = base::android::AttachCurrentThread();
  callback_ = std::move(callback);
  std::vector<std::string> header_keys;
  std::vector<std::string> header_values;
  header_keys.reserve(headers.size());
  header_values.reserve(headers.size());
  for (const auto& [key, value] : headers) {
    header_keys.push_back(key);
    header_values.push_back(value);
  }
  Java_IdentityProviderService_fetch(env, java_obj_, url, body, header_keys,
                                     header_values);
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
  if (callback_) {
    std::move(callback_).Run(data);
  }
}

void IdentityProviderService::OnConnected(JNIEnv* env, bool success) {
  if (connect_callback_) {
    std::move(connect_callback_).Run(success);
  }
}

void IdentityProviderService::OnDisconnected(JNIEnv* env) {
  auto connect_callback = std::move(connect_callback_);
  auto callback = std::move(callback_);
  auto disconnect_callback = std::move(disconnect_callback_);

  // If the service disconnects/dies while a fetch or connection is in flight,
  // resolve those callbacks with an error so the caller doesn't hang forever.
  if (connect_callback) {
    std::move(connect_callback).Run(false);
  }
  if (callback) {
    std::move(callback).Run(std::nullopt);
  }
  if (disconnect_callback) {
    std::move(disconnect_callback).Run();
  }
}

DEFINE_JNI(IdentityProviderService)

}  // namespace content::webid
