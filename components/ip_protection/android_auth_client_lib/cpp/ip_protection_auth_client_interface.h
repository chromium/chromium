// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_INTERFACE_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_INTERFACE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/auth_and_sign.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/get_initial_data.pb.h"

namespace ip_protection::android {

// Interface for wrapper around the Java IpProtectionAuthClient that translates
// native function calls into IPCs to the Android service implementing IP
// Protection.
class IpProtectionAuthClientInterface {
 public:
  virtual ~IpProtectionAuthClientInterface() = default;

  // Asynchronously send a GetInitialDataRequest to the signing server.
  virtual void GetInitialData(
      const privacy::ppn::GetInitialDataRequest& request,
      base::OnceCallback<void(
          base::expected<privacy::ppn::GetInitialDataResponse, std::string>)>
          callback) const = 0;

  // Asynchronously send an AuthAndSignRequest to the signing server.
  virtual void AuthAndSign(
      const privacy::ppn::AuthAndSignRequest& request,
      base::OnceCallback<void(base::expected<privacy::ppn::AuthAndSignResponse,
                                             std::string>)> callback) const = 0;
};

}  // namespace ip_protection::android

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_AUTH_CLIENT_LIB_CPP_IP_PROTECTION_AUTH_CLIENT_INTERFACE_H_
