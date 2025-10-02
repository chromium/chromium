// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_DIRECT_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_DIRECT_FETCHER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_token_fetcher.h"
#include "components/ip_protection/common/ip_protection_token_fetcher_helper.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ip_protection {

class IpProtectionConfigHttp;

// An implementation of IpProtectionTokenFetcher that uses HTTP fetching in
// the `quiche::BlindSignAuth` library for retrieving blind-signed
// authentication tokens for IP Protection.
class IpProtectionTokenDirectFetcher : public IpProtectionTokenFetcher {
 public:
  // A delegate to support getting OAuth tokens to authenticate requests.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Checks if IP Protection is disabled via user settings.
    virtual bool IsTokenFetchEnabled() = 0;

    // Calls the IdentityManager asynchronously to request the OAuth token for
    // the logged in user, or nullopt and an error code.
    using RequestOAuthTokenCallback =
        base::OnceCallback<void(TryGetAuthTokensResult result,
                                std::optional<std::string> token)>;
    virtual void RequestOAuthToken(RequestOAuthTokenCallback callback) = 0;
  };

  explicit IpProtectionTokenDirectFetcher(
      Delegate* delegate,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface>
          blind_sign_auth_for_testing = nullptr);

  ~IpProtectionTokenDirectFetcher() override;

  // IpProtectionTokenFetcher implementation:
  void TryGetAuthTokens(uint32_t batch_size,
                        ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;

  // The account status has changed, so the delegate's `RequestOAuthToken`
  // behavior may change.
  void AccountStatusChanged(bool account_available);

 private:
  friend class IpProtectionTokenDirectFetcherTest;

  // The helper runs in `SequenceBound<SequenceBoundFetch>`, and
  // `network::SharedURLLoaderFactory::Create` must be called in that sequence.
  // This class owns the stack of BlindSignAuthInterface ->
  // IpProtectionConfigHttp -> SharedURLLoaderFactory, constructing and
  // destructing them in `thread_pool_task_runner_`.
  class SequenceBoundFetch {
   public:
    SequenceBoundFetch(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                           pending_url_loader_factory,
                       std::unique_ptr<quiche::BlindSignAuthInterface>
                           blind_sign_auth_for_testing);
    ~SequenceBoundFetch();

    // Calls the `IpProtectionTokenFetcherHelper` method of the same name,
    // passing a pointer to the `blind_sign_auth_` in this instance.
    void GetTokensFromBlindSignAuth(
        quiche::BlindSignAuthServiceType service_type,
        std::optional<std::string> access_token,
        uint32_t batch_size,
        quiche::ProxyLayer proxy_layer,
        perfetto::Track track,
        IpProtectionTokenFetcherHelper::FetchBlindSignedTokenCallback callback);

   private:
    // The BlindSignAuth implementation used to fetch blind-signed auth
    // tokens. A raw pointer to `url_loader_factory_` gets passed to
    // `blind_sign_auth_`, so we ensure it stays alive by storing
    // its scoped_refptr here.
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    std::unique_ptr<quiche::BlindSignAuthInterface> blind_sign_auth_;
    IpProtectionTokenFetcherHelper fetcher_helper_;
  };



  void OnRequestOAuthTokenCompletedForTryGetAuthTokens(
      uint32_t batch_size,
      quiche::ProxyLayer quiche_proxy_layer,
      TryGetAuthTokensCallback callback,
      base::TimeTicks oauth_token_fetch_start_time,
      perfetto::Track track,
      TryGetAuthTokensResult result,
      std::optional<std::string> access_token);

  void OnFetchBlindSignedTokenCompleted(
      base::TimeTicks bsa_get_tokens_start_time,
      TryGetAuthTokensCallback callback,
      perfetto::Track track,
      absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens);

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      std::optional<std::vector<BlindSignedAuthToken>> bsa_tokens,
      TryGetAuthTokensCallback callback,
      TryGetAuthTokensResult result,
      perfetto::Track track,
      std::optional<base::TimeDelta> duration = std::nullopt);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  std::optional<base::TimeDelta> CalculateBackoff(
      TryGetAuthTokensResult result);

  raw_ptr<Delegate> delegate_;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`. `last_try_get_auth_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver
  // (so, from either the main profile or an associated incognito mode profile).
  TryGetAuthTokensResult last_try_get_auth_tokens_result_ =
      TryGetAuthTokensResult::kSuccess;
  std::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The thread pool task runner on which BSA token generation takes place,
  // isolating this CPU-intensive process from the UI thread.
  scoped_refptr<base::SequencedTaskRunner> thread_pool_task_runner_;

  // The helper, sequence-bound to `thread_pool_task_runner_`.
  base::SequenceBound<SequenceBoundFetch> helper_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<IpProtectionTokenDirectFetcher> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_TOKEN_DIRECT_FETCHER_H_
