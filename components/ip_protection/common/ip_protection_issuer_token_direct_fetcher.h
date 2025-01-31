// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_ISSUER_TOKEN_DIRECT_FETCHER_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_ISSUER_TOKEN_DIRECT_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/ip_protection/common/ip_protection_issuer_token_fetcher.h"
#include "components/ip_protection/get_issuer_token.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ip_protection {

// TODO(crbug.com/391357128): implement backoff for failed retrieve.
// TODO(crbug.com/391358904): add metrics

// Implements IpProtectionIssuerTokenFetcher abstract base class.
// Main functionality is implemented in TryGetIssuerTokens method.
class IpProtectionIssuerTokenDirectFetcher
    : public IpProtectionIssuerTokenFetcher {
 public:
  // Retriever class fetches issuer tokens from the issuer server.
  class Retriever {
   public:
    explicit Retriever(std::unique_ptr<network::PendingSharedURLLoaderFactory>
                           pending_url_loader_factory);
    ~Retriever();
    // Returns HTTP body string from issuer or url_loader->NetError() code on
    // error.
    using RetrieveCallback = base::OnceCallback<void(
        base::expected<std::optional<std::string>, int>)>;
    void RetrieveIssuerToken(RetrieveCallback callback);

   private:
    void OnRetrieveIssuerTokenCompleted(
        std::unique_ptr<network::SimpleURLLoader> url_loader,
        RetrieveCallback callback,
        std::optional<std::string> response);

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    const network::ResourceRequest request_;
    const std::string request_body_;
    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<Retriever> weak_ptr_factory_{this};
  };

  explicit IpProtectionIssuerTokenDirectFetcher(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          url_loader_factory);

  ~IpProtectionIssuerTokenDirectFetcher() override;

  void TryGetIssuerTokens(TryGetIssuerTokensCallback callback) override;

  void AccountStatusChanged(bool account_available);

  // Timeout for failures from TryGetIssuerTokens. This is doubled for each
  // subsequent failure.
  static constexpr base::TimeDelta kGetIssuerTokensFailureTimeout =
      base::Minutes(1);

 private:
  void OnGetIssuerTokenCompleted(
      TryGetIssuerTokensCallback callback,
      base::expected<std::optional<std::string>, int> response);

  TryGetIssuerTokensStatus ValidateIssuerTokenResponse(
      const GetIssuerTokenResponse& response);

  // Reset the backoff settings to their default (no-error) state.
  void ClearBackoffTimer();

  Retriever retriever_;

  // The time before the retriever's RetrieveIssuerToken should not be called,
  // and the exponential backoff to be applied next time such a call fails.
  base::Time no_get_issuer_tokens_until_;
  base::TimeDelta next_get_issuer_tokens_backoff_ =
      kGetIssuerTokensFailureTimeout;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionIssuerTokenDirectFetcher> weak_ptr_factory_{
      this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_ISSUER_TOKEN_DIRECT_FETCHER_H_
