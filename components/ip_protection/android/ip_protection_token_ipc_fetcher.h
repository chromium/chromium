// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_ANDROID_IP_PROTECTION_TOKEN_IPC_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_ANDROID_IP_PROTECTION_TOKEN_IPC_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/android/blind_sign_message_android_impl.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"

namespace ip_protection {

// An implementation of IpProtectionTokenFetcher that makes IPC calls to
// service implementing IP Protection in the `quiche::BlindSignAuth` library for
// retrieving blind-signed authentication tokens for IP Protection.
class IpProtectionTokenIpcFetcher : public IpProtectionTokenFetcher {
 public:
  explicit IpProtectionTokenIpcFetcher(
      std::unique_ptr<quiche::BlindSignAuthInterface>
          blind_sign_auth_for_testing = nullptr);

  ~IpProtectionTokenIpcFetcher() override;

  // IpProtectionTokenFetcher implementation:
  void FetchBlindSignedToken(std::optional<std::string> access_token,
                             uint32_t batch_size,
                             quiche::ProxyLayer proxy_layer,
                             FetchBlindSignedTokenCallback callback) override;

 private:
  // The BlindSignAuth implementation used to fetch blind-signed auth tokens.
  std::unique_ptr<ip_protection::BlindSignMessageAndroidImpl>
      blind_sign_message_android_impl_;
  std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_ANDROID_IP_PROTECTION_TOKEN_IPC_FETCHER_H_
