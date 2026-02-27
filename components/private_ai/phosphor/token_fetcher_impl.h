// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_FETCHER_IMPL_H_
#define COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_FETCHER_IMPL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/types/expected.h"
#include "components/private_ai/phosphor/blind_sign_auth_factory.h"
#include "components/private_ai/phosphor/data_types.h"
#include "components/private_ai/phosphor/oauth_token_provider.h"
#include "components/private_ai/phosphor/token_fetcher.h"
#include "components/private_ai/phosphor/token_fetcher_helper.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace private_ai::phosphor {

class ConfigHttp;

// An implementation of TokenFetcher that uses HTTP fetching in
// the `quiche::BlindSignAuth` library for retrieving blind-signed
// authentication tokens for PrivateAI.
class TokenFetcherImpl : public TokenFetcher {
 public:
  TokenFetcherImpl(OAuthTokenProvider* oauth_token_provider,
                   std::unique_ptr<quiche::BlindSignAuthInterface> bsa);

  ~TokenFetcherImpl() override;

  // TokenFetcher implementation:
  void GetAuthnTokens(int batch_size,
                      quiche::ProxyLayer proxy_layer,
                      GetAuthnTokensCallback callback) override;

  // The account status has changed, so the delegate's `RequestOAuthToken`
  // behavior may change.
  void AccountStatusChanged(bool account_available);

 private:
  friend class TokenFetcherImplTest;

  // The helper runs in `SequenceBound<SequenceBoundFetch>`, and
  // `network::SharedURLLoaderFactory::Create` must be called in that sequence.
  // This class owns the stack of BlindSignAuthInterface ->
  // ConfigHttp -> SharedURLLoaderFactory, constructing and
  // destructing them in `thread_pool_task_runner_`.
  class SequenceBoundFetch {
   public:
    explicit SequenceBoundFetch(
        std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth);
    ~SequenceBoundFetch();

    // Calls the `TokenFetcherHelper` method of the same name,
    // passing a pointer to the `blind_sign_auth_` in this instance.
    void GetTokensFromBlindSignAuth(
        quiche::BlindSignAuthServiceType service_type,
        std::optional<std::string> access_token,
        int batch_size,
        quiche::ProxyLayer proxy_layer,
        TokenFetcherHelper::FetchBlindSignedTokenCallback callback);

   private:
    std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_;
    TokenFetcherHelper fetcher_helper_;
  };

  void OnRequestOAuthTokenCompletedForGetAuthnTokens(
      base::TimeTicks start_time,
      int batch_size,
      quiche::ProxyLayer proxy_layer,
      GetAuthnTokensCallback callback,
      GetAuthnTokensResult result,
      std::optional<std::string> access_token);

  void OnFetchBlindSignedTokenCompleted(
      GetAuthnTokensCallback callback,
      base::expected<std::vector<quiche::BlindSignToken>, absl::Status> tokens);

  // Finish a call to `GetAuthnTokens()` by recording the result and invoking
  // its callback.
  void GetAuthnTokensComplete(
      std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens,
      GetAuthnTokensCallback callback,
      GetAuthnTokensResult result);

  // Calculates the backoff time for the given result, based on
  // `last_get_authn_tokens_..` fields, and updates those fields.
  std::optional<base::TimeDelta> CalculateBackoff(GetAuthnTokensResult result);

  raw_ptr<OAuthTokenProvider> oauth_token_provider_;

  // The result of the last call to `GetAuthnTokens()`, and the
  // backoff applied to `try_again_after`. `last_get_authn_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver
  // (so, from either the main profile or an associated incognito mode profile).
  GetAuthnTokensResult last_get_authn_tokens_result_ =
      GetAuthnTokensResult::kSuccess;
  std::optional<base::TimeDelta> last_get_authn_tokens_backoff_;

  // The thread pool task runner on which BSA token generation takes place,
  // isolating this CPU-intensive process from the UI thread.
  scoped_refptr<base::SequencedTaskRunner> thread_pool_task_runner_;

  // The helper, sequence-bound to `thread_pool_task_runner_`.
  base::SequenceBound<SequenceBoundFetch> helper_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TokenFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace private_ai::phosphor

#endif  // COMPONENTS_PRIVATE_AI_PHOSPHOR_TOKEN_FETCHER_IMPL_H_
