// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_DIRECT_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_DIRECT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/version_info/channel.h"
#include "components/ip_protection/common/ip_protection_probabilistic_reveal_token_fetcher.h"
#include "components/ip_protection/get_probabilistic_reveal_token.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ip_protection {

// Implements IpProtectionProbabilisticRevealTokenFetcher abstract base class.
// Main functionality is implemented in TryGetProbabilisticRevealTokens method.
class IpProtectionProbabilisticRevealTokenDirectFetcher
    : public IpProtectionProbabilisticRevealTokenFetcher {
 public:
  // Retriever class fetches probabilistic reveal tokens from the issuer server.
  class Retriever {
   public:
    Retriever(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                  pending_url_loader_factory,
              version_info::Channel channel);
    ~Retriever();
    // Returns HTTP body string from issuer or url_loader->NetError() code on
    // error.
    using RetrieveCallback = base::OnceCallback<void(
        base::expected<std::optional<std::string>, int>)>;
    void RetrieveProbabilisticRevealTokens(RetrieveCallback callback);

   private:
    void OnRetrieveProbabilisticRevealTokensCompleted(
        std::unique_ptr<network::SimpleURLLoader> url_loader,
        RetrieveCallback callback,
        std::optional<std::string> response);

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    const network::ResourceRequest request_;
    const std::string request_body_;
    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<Retriever> weak_ptr_factory_{this};
  };

  IpProtectionProbabilisticRevealTokenDirectFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory,
      version_info::Channel channel);

  ~IpProtectionProbabilisticRevealTokenDirectFetcher() override;

  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override;

  void AccountStatusChanged(bool account_available);

  // Timeout for failures from TryGetProbabilisticRevealTokens. This is doubled
  // for each subsequent failure.
  static constexpr base::TimeDelta kGetProbabilisticRevealTokensFailureTimeout =
      base::Minutes(1);

 private:
  void OnGetProbabilisticRevealTokensCompleted(
      TryGetProbabilisticRevealTokensCallback callback,
      base::expected<std::optional<std::string>, int> response);

  TryGetProbabilisticRevealTokensStatus
  ValidateProbabilisticRevealTokenResponse(
      const GetProbabilisticRevealTokenResponse& response);

  // Reset the backoff settings to their default (no-error) state.
  void ClearBackoffTimer();

  Retriever retriever_;

  // The time before the retriever's RetrieveProbabilisticRevealTokens should
  // not be called, and the exponential backoff to be applied next time such a
  // call fails.
  base::Time no_get_probabilistic_reveal_tokens_until_;
  base::TimeDelta next_get_probabilistic_reveal_tokens_backoff_ =
      kGetProbabilisticRevealTokensFailureTimeout;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionProbabilisticRevealTokenDirectFetcher>
      weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROBABILISTIC_REVEAL_TOKEN_DIRECT_FETCHER_H_
