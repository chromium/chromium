// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/android/ip_protection_token_ipc_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/sequence_checker.h"
#include "components/ip_protection/android/blind_sign_message_android_impl.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"

namespace ip_protection {

IpProtectionTokenIpcFetcher::IpProtectionTokenIpcFetcher(
    std::unique_ptr<quiche::BlindSignAuthInterface>
        blind_sign_auth_for_testing) {
  blind_sign_message_android_impl_ =
      std::make_unique<ip_protection::BlindSignMessageAndroidImpl>();
  // TODO(b/360340499) : Remove `blind_sign_auth_for_testing` and implement mock
  // fetcher for unit tests.
  if (blind_sign_auth_for_testing) {
    blind_sign_auth_ = std::move(blind_sign_auth_for_testing);
    return;
  }
  privacy::ppn::BlindSignAuthOptions bsa_options{};
  bsa_options.set_enable_privacy_pass(true);

  blind_sign_auth_ = std::make_unique<quiche::BlindSignAuth>(
      blind_sign_message_android_impl_.get(), std::move(bsa_options));
}

IpProtectionTokenIpcFetcher::~IpProtectionTokenIpcFetcher() = default;

void IpProtectionTokenIpcFetcher::FetchBlindSignedToken(
    std::optional<std::string> access_token,
    uint32_t batch_size,
    quiche::ProxyLayer proxy_layer,
    FetchBlindSignedTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IpProtectionTokenFetcher::GetTokensFromBlindSignAuth(
      blind_sign_auth_.get(),
      quiche::BlindSignAuthServiceType::kWebviewIpBlinding,
      std::move(access_token), batch_size, proxy_layer, std::move(callback));
}

}  // namespace ip_protection
