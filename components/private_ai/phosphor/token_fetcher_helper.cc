// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/phosphor/token_fetcher_helper.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/proto/private_ai.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"

namespace private_ai::phosphor {

TokenFetcherHelper::TokenFetcherHelper() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void TokenFetcherHelper::GetTokensFromBlindSignAuth(
    quiche::BlindSignAuthInterface* blind_sign_auth,
    quiche::BlindSignAuthServiceType service_type,
    std::optional<std::string> access_token,
    int batch_size,
    quiche::ProxyLayer proxy_layer,
    FetchBlindSignedTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blind_sign_auth->GetTokens(
      std::move(access_token), batch_size, proxy_layer, service_type,
      [callback = std::move(callback)](auto tokens) mutable {
        if (tokens.ok()) {
          std::move(callback).Run(base::ok(std::vector<quiche::BlindSignToken>(
              tokens->begin(), tokens->end())));
        } else {
          std::move(callback).Run(base::unexpected(tokens.status()));
        }
      });
}

// static
std::optional<std::vector<BlindSignedAuthToken>>
TokenFetcherHelper::QuicheTokensToPhosphorAuthTokens(
    std::vector<quiche::BlindSignToken>& tokens) {
  std::vector<BlindSignedAuthToken> bsa_tokens;
  for (const quiche::BlindSignToken& token : tokens) {
    std::optional<BlindSignedAuthToken> converted_token =
        TokenFetcherHelper::CreateBlindSignedAuthToken(token);
    if (!converted_token.has_value() || converted_token->token.empty()) {
      VLOG(2) << "Failed to convert `quiche::BlindSignToken` to a "
                 "`phosphor::BlindSignedAuthToken`";
      return std::nullopt;
    } else {
      bsa_tokens.push_back(std::move(converted_token).value());
    }
  }
  return bsa_tokens;
}

// static
std::optional<BlindSignedAuthToken>
TokenFetcherHelper::CreateBlindSignedAuthToken(
    const quiche::BlindSignToken& bsa_token) {
  // Set expiration of BlindSignedAuthToken.
  base::Time expiration =
      base::Time::FromTimeT(absl::ToTimeT(bsa_token.expiration));

  // Set token of BlindSignedAuth Token to be the fully constructed
  // authorization header value.
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;
  if (!privacy_pass_token_data.ParseFromString(bsa_token.token)) {
    return std::nullopt;
  }

  if (privacy_pass_token_data.token().empty() ||
      privacy_pass_token_data.encoded_extensions().empty()) {
    VLOG(2) << "PrivacyPassTokenData is missing fields";
    return std::nullopt;
  }

  if (!base::ContainsOnlyChars(privacy_pass_token_data.encoded_extensions(),
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz"
                               "0123456789+/=")) {
    VLOG(2) << "Invalid base64 characters in encoded_extensions";
    return std::nullopt;
  }

  return std::make_optional<BlindSignedAuthToken>(
      {.token = privacy_pass_token_data.token(),
       .encoded_extensions = privacy_pass_token_data.encoded_extensions(),
       .expiration = expiration});
}

}  // namespace private_ai::phosphor
