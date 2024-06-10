// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/ip_protection/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace ip_protection::android {

class IpProtectionAuthClient;

// Used to return an IpProtectionAuthClient or error to the user.
// Expected type won't change, error type will be updated.
using CreateIpProtectionAuthClientCallback = base::OnceCallback<void(
    base::expected<std::unique_ptr<IpProtectionAuthClientInterface>,
                   std::string>)>;

// Used to return a GetInitialDataResponse or error to the user.
// Expected type won't change, error type will be updated.
using GetInitialDataResponseCallback = base::OnceCallback<void(
    base::expected<privacy::ppn::GetInitialDataResponse, std::string>)>;

// Used to return an AuthAndSignResponse or error to the user.
// Expected type won't change, error type will be updated.
using AuthAndSignResponseCallback = base::OnceCallback<void(
    base::expected<privacy::ppn::AuthAndSignResponse, std::string>)>;

// Wrapper around the Java IpProtectionAuthClient that translates native
// function calls into IPCs to the Android service implementing IP Protection.
// TODO(b/328781171): replace std::string error messages with an ErrorCode enum
class IpProtectionAuthClient : public IpProtectionAuthClientInterface {
 public:
  ~IpProtectionAuthClient() override;
  IpProtectionAuthClient(const IpProtectionAuthClient& other) = delete;
  IpProtectionAuthClient& operator=(const IpProtectionAuthClient& other) =
      delete;

  // Asynchronously request to bind to the Android IP Protection service.
  // Callback will be invoked on the calling process's main thread.
  static void CreateConnectedInstance(
      CreateIpProtectionAuthClientCallback callback);

  // Request to bind to the mock Android IP Protection service.
  // This function should only be called in tests.
  // Callback will be invoked on the calling process's main thread.
  static void CreateMockConnectedInstance(
      CreateIpProtectionAuthClientCallback callback);

  // Asynchronously send a GetInitialDataRequest to the signing server.
  // Callback will be invoked on a thread from the Binder thread pool.
  void GetInitialData(const privacy::ppn::GetInitialDataRequest& request,
                      GetInitialDataResponseCallback callback) const override;

  // Asynchronously send an AuthAndSignRequest to the signing server.
  // Callback will be invoked on a thread from the Binder thread pool.
  void AuthAndSign(const privacy::ppn::AuthAndSignRequest& request,
                   AuthAndSignResponseCallback callback) const override;

 private:
  // BindCallbackListener::OnResult calls IpProtectionAuthClient's constructor.
  friend class BindCallbackListener;
  explicit IpProtectionAuthClient(
      const jni_zero::JavaRef<jobject>& ip_protection_auth_client);

  template <typename T>
  base::OnceCallback<void(base::expected<std::string, std::string>)>
  ConvertProtoCallback(
      base::OnceCallback<void(base::expected<T, std::string>)> callback) const;

  // Reference to the Java IpProtectionAuthClient object.
  jni_zero::ScopedJavaGlobalRef<jobject> ip_protection_auth_client_;
};

}  // namespace ip_protection::android

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_H_
