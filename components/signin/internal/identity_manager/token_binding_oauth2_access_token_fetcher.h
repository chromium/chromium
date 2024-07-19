// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_OAUTH2_ACCESS_TOKEN_FETCHER_H_
#define COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_OAUTH2_ACCESS_TOKEN_FETCHER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/base/hybrid_encryption_key.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"

class OAuth2MintAccessTokenFetcherAdapter;

// OAuth2AccessTokenFetcher that waits for a binding key assertion being
// generated before starting an actual fetch that is delegated to a fetcher
// instance passed in the constructor.
//
// `Start()` will not trigger the actual fetcher until
// `SetBindingKeyAssertion()` is called.
class TokenBindingOAuth2AccessTokenFetcher : public OAuth2AccessTokenFetcher {
 public:
  explicit TokenBindingOAuth2AccessTokenFetcher(
      std::unique_ptr<OAuth2MintAccessTokenFetcherAdapter> fetcher);

  ~TokenBindingOAuth2AccessTokenFetcher() override;

  // Appends `assertion` to the fetch request and unblocks `Start()`:
  // - if `Start()` was called prior to this method, starts a new fetch;
  // - otherwise, the next `Start()` call will start a new fetch immediately.
  // An empty `assertion` signifies that the assertion generation failed.
  // `ephemeral_key`, if set, should be used to decrypt credentials in the
  // response message. The key is ignored if `assertion` is empty.
  void SetBindingKeyAssertion(std::string assertion,
                              std::optional<HybridEncryptionKey> ephemeral_key);

  // OAuth2AccessTokenFetcher:
  void Start(const std::string& client_id,
             const std::string& client_secret,
             const std::vector<std::string>& scopes) override;
  void CancelRequest() override;

  base::WeakPtr<TokenBindingOAuth2AccessTokenFetcher> GetWeakPtr();

 private:
  void MaybeStartFetch();

  // Underlying `OAuth2AccessTokenFetcher` that performs the actual fetch.
  // In theory, this could support any implementation of the base
  // `OAuth2AccessTokenFetcher` class that has a binding key assertion setter.
  const std::unique_ptr<OAuth2MintAccessTokenFetcherAdapter> fetcher_;

  // The callback holds parameters that were earlier passed to `Start()`.
  base::OnceCallback<void(OAuth2AccessTokenFetcher*)> delayed_fetcher_start_;
  bool is_binding_key_assertion_set_ = false;

  base::WeakPtrFactory<TokenBindingOAuth2AccessTokenFetcher> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_SIGNIN_INTERNAL_IDENTITY_MANAGER_TOKEN_BINDING_OAUTH2_ACCESS_TOKEN_FETCHER_H_
