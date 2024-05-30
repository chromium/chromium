// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client.h"

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "components/ip_protection/android_auth_client_lib/cpp/bind_callback_listener.h"
#include "components/ip_protection/android_auth_client_lib/cpp/byte_array_callback_listener.h"
#include "components/ip_protection/android_auth_client_lib/cpp/jni_headers/IpProtectionAuthClient_jni.h"

namespace ip_protection::android {

void IpProtectionAuthClient::CreateConnectedInstance(
    CreateIpProtectionAuthClientCallback callback) {
  Java_IpProtectionAuthClient_createConnectedInstance(
      base::android::AttachCurrentThread(),
      BindCallbackListener::Create(std::move(callback)));
}

void IpProtectionAuthClient::CreateMockConnectedInstance(
    CreateIpProtectionAuthClientCallback callback) {
  Java_IpProtectionAuthClient_createConnectedInstanceForTestingAsync(
      base::android::AttachCurrentThread(),
      BindCallbackListener::Create(std::move(callback)));
}

void IpProtectionAuthClient::GetInitialData(
    const privacy::ppn::GetInitialDataRequest& request,
    GetInitialDataResponseCallback callback) const {
  Java_IpProtectionAuthClient_getInitialData(
      base::android::AttachCurrentThread(), ip_protection_auth_client_,
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     request.SerializeAsString()),
      ByteArrayCallbackListener::Create(
          ConvertProtoCallback<privacy::ppn::GetInitialDataResponse>(
              std::move(callback))));
}

void IpProtectionAuthClient::AuthAndSign(
    const privacy::ppn::AuthAndSignRequest& request,
    AuthAndSignResponseCallback callback) const {
  Java_IpProtectionAuthClient_authAndSign(
      base::android::AttachCurrentThread(), ip_protection_auth_client_,
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     request.SerializeAsString()),
      ByteArrayCallbackListener::Create(
          ConvertProtoCallback<privacy::ppn::AuthAndSignResponse>(
              std::move(callback))));
}

IpProtectionAuthClient::IpProtectionAuthClient(
    const jni_zero::JavaRef<jobject>& ip_protection_auth_client)
    : ip_protection_auth_client_(ip_protection_auth_client) {}

IpProtectionAuthClient::~IpProtectionAuthClient() {
  Java_IpProtectionAuthClient_close(base::android::AttachCurrentThread(),
                                    ip_protection_auth_client_);
}

template <typename T>
base::OnceCallback<void(base::expected<std::string, std::string>)>
IpProtectionAuthClient::ConvertProtoCallback(
    base::OnceCallback<void(base::expected<T, std::string>)> callback) const {
  return base::BindOnce(
      [](base::OnceCallback<void(base::expected<T, std::string>)> callback,
         base::expected<std::string, std::string> response) {
        if (!response.has_value()) {
          std::move(callback).Run(
              base::unexpected(std::move(response).error()));
        } else {
          T response_proto;
          response_proto.ParseFromString(*response);
          std::move(callback).Run(std::move(response_proto));
        }
      },
      std::move(callback));
}

}  // namespace ip_protection::android
