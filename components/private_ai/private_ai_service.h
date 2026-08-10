// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_PRIVATE_AI_SERVICE_H_
#define COMPONENTS_PRIVATE_AI_PRIVATE_AI_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/private_ai/client.h"
#include "components/private_ai/common/private_ai_logger.h"
#include "components/private_ai/phosphor/oauth_token_provider.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network::mojom {
class NetworkContext;
}

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}

namespace version_info {
enum class Channel;
}

namespace private_ai {

namespace phosphor {
class BlindSignAuthFactory;
class TokenFetcherImpl;
class TokenManager;
}  // namespace phosphor

class PrivateAiNetworkDriver;
class PrivateAiOakSessionDriver;

// The `PrivateAiService` is a KeyedService responsible for managing
// authentication tokens for the PrivateAI feature. It observes the user's
// sign-in state and, when a primary account is available, it can fetch OAuth2
// access tokens. These access tokens are then used by the underlying
// `phosphor::TokenManager` and `phosphor::TokenFetcher` to acquire and manage
// authentication tokens for PrivateAI. This class is concrete and shared
// across platforms, with platform-specific behavior injected via drivers.
class PrivateAiService : public KeyedService,
                         public phosphor::OAuthTokenProvider,
                         public signin::IdentityManager::Observer {
 public:
  PrivateAiService(
      signin::IdentityManager* identity_manager,
      phosphor::BlindSignAuthFactory* bsa_factory,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<PrivateAiNetworkDriver> network_driver,
      std::unique_ptr<PrivateAiOakSessionDriver> oak_session_driver,
      network::mojom::NetworkContext* network_context,
      const std::string& url,
      const std::string& api_key,
      const std::string& proxy_url,
      bool use_token_attestation,
      version_info::Channel channel);
  ~PrivateAiService() override;

  PrivateAiService(const PrivateAiService&) = delete;
  PrivateAiService& operator=(const PrivateAiService&) = delete;

  // Returns the API key for the Private AI feature.
  static std::string GetApiKey(version_info::Channel channel);

  // Returns whether the Private AI feature can be enabled.
  static bool CanPrivateAiBeEnabled(version_info::Channel channel);

  // KeyedService override:
  void Shutdown() override;

  // Returns `nullptr` if `PrivateAiService` is shutting down.
  phosphor::TokenManager* GetTokenManager();

  Client* GetClient();

  PrivateAiLogger* GetLogger();

  void SetClientForTesting(std::unique_ptr<Client> client);

  // phosphor::OAuthTokenProvider override:
  bool IsTokenFetchEnabled() override;
  void RequestOAuthToken(RequestOAuthTokenCallback callback) override;

 private:
  void OnRequestOAuthTokenCompleted(
      std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
      RequestOAuthTokenCallback callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);

  // signin::IdentityManager::Observer override:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<signin::IdentityManager> identity_manager_;

  PrivateAiLogger logger_;

  std::unique_ptr<phosphor::TokenManager> token_manager_;
  // Owned by `token_manager_`.
  raw_ptr<phosphor::TokenFetcherImpl> token_fetcher_ = nullptr;

  std::unique_ptr<PrivateAiNetworkDriver> network_driver_;
  std::unique_ptr<PrivateAiOakSessionDriver> oak_session_driver_;

  std::unique_ptr<Client> client_;

  bool is_shutting_down_ = false;

  base::WeakPtrFactory<PrivateAiService> weak_ptr_factory_{this};
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_PRIVATE_AI_SERVICE_H_
