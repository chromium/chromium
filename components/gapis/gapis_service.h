// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAPIS_GAPIS_SERVICE_H_
#define COMPONENTS_GAPIS_GAPIS_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/cdm_helper.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace gapis {

class TokenDownloader;

class GapisService : public KeyedService {
 public:
  explicit GapisService(
      signin::IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel);
  ~GapisService() override;

  // Initiates the app token fetch process if not ready yet.
  void FetchAppTokenIfNeeded();

 private:
  enum class CdmInitializationState {
    kNotInitialized,
    kInitializedSuccessfully,
    kFailedToInitialize,
  };

  void InitializeCdm();
  void OnCdmInitialized(content::CdmHelper::InitializeResult result);

  void FetchChallenge();
  void OnChallengeFetched(const std::string& challenge_init_data_hex);
  void SignChallenge(const std::string& challenge);
  void OnChallengeSigned(const std::string& signed_challenge,
                         content::CdmHelper::SignChallengeResult result);

  void OnAccessTokenFetched(const std::string& challenge_init_data_hex,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  void OnAppTokenFetched(const std::string& app_token);

  const raw_ref<signin::IdentityManager> identity_manager_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const GURL service_url_;

  std::unique_ptr<content::CdmHelper> cdm_helper_;
  CdmInitializationState cdm_initialization_state_ =
      CdmInitializationState::kNotInitialized;

  // Pending request for an access token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      ongoing_access_token_fetch_;

  // Pending request for obtaining a token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<TokenDownloader> ongoing_app_token_downloader_;
};

}  // namespace gapis

#endif  // COMPONENTS_GAPIS_GAPIS_SERVICE_H_
