// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/token_binding_oauth2_access_token_fetcher.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "components/signin/public/base/session_binding_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_mint_access_token_fetcher_adapter.h"

TokenBindingOAuth2AccessTokenFetcher::TokenBindingOAuth2AccessTokenFetcher(
    std::unique_ptr<OAuth2MintAccessTokenFetcherAdapter> fetcher)
    : OAuth2AccessTokenFetcher(
          /*consumer=*/nullptr),  // consumer should be passed to `fetcher`
      fetcher_(std::move(fetcher)) {
  CHECK(fetcher_);
}

TokenBindingOAuth2AccessTokenFetcher::~TokenBindingOAuth2AccessTokenFetcher() =
    default;

void TokenBindingOAuth2AccessTokenFetcher::SetBindingKeyAssertion(
    std::optional<HybridEncryptionKey> ephemeral_key,
    std::string assertion) {
  if (assertion.empty()) {
    // Even if the assertion failed, we want to make a server request because
    // the server doesn't verify assertions during dark launch.
    // TODO(crbug.com/377942773): fail here immediately after the feature is
    // fully launched.
    assertion = GaiaConstants::kTokenBindingAssertionFailedPlaceholder;
  } else if (ephemeral_key.has_value()) {
    fetcher_->SetTokenDecryptor(
        base::BindRepeating(&signin::DecryptValueWithEphemeralKey,
                            std::move(ephemeral_key).value()));
  }
  fetcher_->SetBindingKeyAssertion(std::move(assertion));
  is_binding_key_assertion_set_ = true;
  MaybeStartFetch();
}

void TokenBindingOAuth2AccessTokenFetcher::Start(
    const std::string& client_id,
    const std::string& client_secret,
    const std::vector<std::string>& scopes) {
  // Store Start() parameters in the callback bind state to pass them into
  // `fetcher_` later.
  delayed_fetcher_start_ = base::BindOnce(
      [](const std::string& client_id, const std::string& client_secret,
         const std::vector<std::string>& scopes,
         OAuth2AccessTokenFetcher* fetcher) {
        fetcher->Start(client_id, client_secret, scopes);
      },
      client_id, client_secret, scopes);
  MaybeStartFetch();
}

void TokenBindingOAuth2AccessTokenFetcher::CancelRequest() {
  fetcher_->CancelRequest();
}

base::WeakPtr<TokenBindingOAuth2AccessTokenFetcher>
TokenBindingOAuth2AccessTokenFetcher::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TokenBindingOAuth2AccessTokenFetcher::MaybeStartFetch() {
  if (delayed_fetcher_start_ && is_binding_key_assertion_set_) {
    std::move(delayed_fetcher_start_).Run(fetcher_.get());
  }
}
