// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_INTERFACE_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_INTERFACE_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/ip_protection/get_proxy_config.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace ip_protection::android {

// Errors codes which may be passed to GetInitialData and AuthAndSign response
// callbacks.
//
// These values must be kept in sync with AuthRequestError in
// IpProtectionAuthClient.java
enum class AuthRequestError {
  // Service explicitly signaled a transient failure, hinting that the operation
  // can be retried.
  kTransient = 0,
  // Service explicitly signaled a persistent failure, hinting that the
  // operation should not be retried.
  kPersistent = 1,
  // There was some failure not explicitly communicated by the service, such as
  // a breakdown in the IPC or an API contract violation.
  kOther = 2,
};

// Used to return a GetInitialDataResponse or error to the user.
using GetInitialDataResponseCallback = base::OnceCallback<void(
    base::expected<privacy::ppn::GetInitialDataResponse, AuthRequestError>)>;

// Used to return an AuthAndSignResponse or error to the user.
using AuthAndSignResponseCallback = base::OnceCallback<void(
    base::expected<privacy::ppn::AuthAndSignResponse, AuthRequestError>)>;

// Used to return an GetProxyConfigResponse or error to the user.
using GetProxyConfigResponseCallback = base::OnceCallback<void(
    base::expected<GetProxyConfigResponse, AuthRequestError>)>;

// Interface for wrapper around the Java IpProtectionAuthClient that translates
// native function calls into IPCs to the Android service implementing IP
// Protection.
class IpProtectionAuthClientInterface {
 public:
  // Supplied to a client factory to asynchronously return an
  // IpProtectionAuthClientInterface or error back to the caller.
  using ClientCreated =
      void(base::expected<std::unique_ptr<IpProtectionAuthClientInterface>,
                          std::string>);

  virtual ~IpProtectionAuthClientInterface() = default;

  // Asynchronously send a GetInitialDataRequest to the signing server.
  virtual void GetInitialData(
      const privacy::ppn::GetInitialDataRequest& request,
      GetInitialDataResponseCallback callback) const = 0;

  // Asynchronously send an AuthAndSignRequest to the signing server.
  virtual void AuthAndSign(const privacy::ppn::AuthAndSignRequest& request,
                           AuthAndSignResponseCallback callback) const = 0;

  // Asynchronously send an GetProxyConfigRequest to the server.
  virtual void GetProxyConfig(
      const GetProxyConfigRequest& request,
      GetProxyConfigResponseCallback callback) const = 0;

  // Returns a weak pointer to this object.
  virtual base::WeakPtr<IpProtectionAuthClientInterface> GetWeakPtr() = 0;
};

}  // namespace ip_protection::android

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_INTERFACE_H_
