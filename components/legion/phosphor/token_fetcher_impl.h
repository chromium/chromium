// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_PHOSPHOR_TOKEN_FETCHER_IMPL_H_
#define COMPONENTS_LEGION_PHOSPHOR_TOKEN_FETCHER_IMPL_H_

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
#include "components/legion/phosphor/data_types.h"
#include "components/legion/phosphor/token_fetcher.h"
#include "components/legion/phosphor/token_fetcher_helper.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace legion::phosphor {

class ConfigHttp;

// An implementation of TokenFetcher that uses HTTP fetching in
// the `quiche::BlindSignAuth` library for retrieving blind-signed
// authentication tokens for Legion.
class TokenFetcherImpl : public TokenFetcher {
 public:
  // A delegate to support getting OAuth tokens to authenticate requests.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Checks if token fetching is enabled.
    virtual bool IsTokenFetchEnabled() = 0;

    // Calls the IdentityManager asynchronously to request the OAuth token for
    // the logged in user, or nullopt and an error code.
    using RequestOAuthTokenCallback =
        base::OnceCallback<void(GetAuthnTokensResult result,
                                std::optional<std::string> token)>;
    virtual void RequestOAuthToken(RequestOAuthTokenCallback callback) = 0;

    // Creates a `quiche::BlindSignAuthInterface` instance. Can be overridden
    // by tests to provide a mock.
    virtual std::unique_ptr<quiche::BlindSignAuthInterface> CreateBlindSignAuth(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  };

  explicit TokenFetcherImpl(
      Delegate* delegate,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_url_loader_factory);

  ~TokenFetcherImpl() override;

  // TokenFetcher implementation:
  void GetAuthnTokens(int batch_size, GetAuthnTokensCallback callback) override;

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
      int batch_size,
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

  raw_ptr<Delegate> delegate_;

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

}  // namespace legion::phosphor

#endif  // COMPONENTS_LEGION_PHOSPHOR_TOKEN_FETCHER_IMPL_H_
