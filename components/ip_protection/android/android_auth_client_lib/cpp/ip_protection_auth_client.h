// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/ip_protection/android/android_auth_client_lib/cpp/ip_protection_auth_client_interface.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace ip_protection::android {

// Wrapper around the Java IpProtectionAuthClient that translates native
// function calls into IPCs to the Android service implementing IP Protection.
class IpProtectionAuthClient : public IpProtectionAuthClientInterface {
 public:
  ~IpProtectionAuthClient() override;
  IpProtectionAuthClient(const IpProtectionAuthClient& other) = delete;
  IpProtectionAuthClient& operator=(const IpProtectionAuthClient& other) =
      delete;

  // Asynchronously request to bind to the Android IP Protection auth service.
  // Callback will be invoked on the calling process's main thread.
  static void CreateConnectedInstance(
      base::OnceCallback<ClientCreated> callback);

  // Request to bind to an alternative or mock Android IP Protection auth
  // service specified by |packageName| and |className|, which identify the
  // component of the service to bind to. The service does not need to be
  // system-installed. Callback will be invoked on the calling process's main
  // thread.
  static void CreateConnectedInstanceForTesting(
      std::string_view packageName,
      std::string_view className,
      base::OnceCallback<ClientCreated> callback);

  // Asynchronously send a GetInitialDataRequest to the signing server.
  //
  // There are no guarantees as to which thread the callback is invoked on. It
  // could be the main thread, a binder thread, some internal sequence, or even
  // be called synchronously! It is the responsibility of the caller to repost
  // to a well-defined sequence as needed (such as via base::BindPostTask or
  // base::BindPostTaskToCurrentDefault).
  void GetInitialData(const privacy::ppn::GetInitialDataRequest& request,
                      GetInitialDataResponseCallback callback) const override;

  // Asynchronously send an AuthAndSignRequest to the signing server.
  //
  // There are no guarantees as to which thread the callback is invoked on. It
  // could be the main thread, a binder thread, some internal sequence, or even
  // be called synchronously! It is the responsibility of the caller to repost
  // to a well-defined sequence as needed (such as via base::BindPostTask or
  // base::BindPostTaskToCurrentDefault).
  void AuthAndSign(const privacy::ppn::AuthAndSignRequest& request,
                   AuthAndSignResponseCallback callback) const override;

  // Asynchronously send an GetProxyConfigRequest to the server.
  //
  // There are no guarantees as to which thread the callback is invoked on. It
  // could be the main thread, a binder thread, some internal sequence, or even
  // be called synchronously! It is the responsibility of the caller to repost
  // to a well-defined sequence as needed (such as via base::BindPostTask or
  // base::BindPostTaskToCurrentDefault).
  void GetProxyConfig(const GetProxyConfigRequest& request,
                      GetProxyConfigResponseCallback callback) const override;

  base::WeakPtr<IpProtectionAuthClientInterface> GetWeakPtr() override;

 private:
  // BindCallbackListener::OnResult calls IpProtectionAuthClient's constructor.
  friend class BindCallbackListener;
  explicit IpProtectionAuthClient(
      const jni_zero::JavaRef<jobject>& ip_protection_auth_client);

  // Reference to the Java IpProtectionAuthClient object.
  jni_zero::ScopedJavaGlobalRef<jobject> ip_protection_auth_client_;

  base::WeakPtrFactory<IpProtectionAuthClient> weak_ptr_factory_{this};
};

}  // namespace ip_protection::android

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_H_
