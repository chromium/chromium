// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gapis/gapis_service.h"

#include <cstdint>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/gapis/features.h"
#include "components/gapis/gapis_manager.h"
#include "components/gapis/token_downloader.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/sync/base/sync_util.h"
#include "components/version_info/channel.h"
#include "content/public/browser/cdm_helper.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace gapis {

// When enabled, GapisService will use CDM to sign the challenge.
BASE_FEATURE(kGapisSignChallenge, base::FEATURE_ENABLED_BY_DEFAULT);

constexpr char kCdmKeySystem[] = "com.google.widevine.alpha";

GapisService::GapisService(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      service_url_(
          syncer::GetSyncServiceURL(*base::CommandLine::ForCurrentProcess(),
                                    channel)) {
  CHECK(base::FeatureList::IsEnabled(kEnableGapis));
  DVLOG(1) << "GapisService URL: " << service_url_;
}

GapisService::~GapisService() = default;

void GapisService::FetchAppTokenIfNeeded() {
  if (ongoing_access_token_fetch_ || ongoing_app_token_downloader_) {
    // Access token or app token fetch is already in progress.
    return;
  }

  if (cdm_initialization_state_ ==
      CdmInitializationState::kFailedToInitialize) {
    // CDM initialization failed.
    return;
  }

  if (base::FeatureList::IsEnabled(kGapisSignChallenge)) {
    if (!cdm_helper_) {
      // First, initialize the CDM, and then fetch the challenge.
      DVLOG(1) << "Initializing CDM";
      InitializeCdm();
      return;
    }

    if (cdm_initialization_state_ == CdmInitializationState::kNotInitialized) {
      // CDM initialization is still in progress.
      return;
    }
  }

  FetchChallenge();
}

void GapisService::InitializeCdm() {
  CHECK(base::FeatureList::IsEnabled(kGapisSignChallenge));
  cdm_helper_ = content::CdmHelper::CreateInstance();

  std::string cdm_server_certificate;
#if BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  cdm_server_certificate = google_apis::GetCdmServerCertificate();
#endif  // BUILDFLAG(SUPPORT_CDM_SERVER_CERTIFICATE)
  cdm_helper_->Initialize(
      cdm_server_certificate, kCdmKeySystem,
      base::BindOnce(&GapisService::OnCdmInitialized, base::Unretained(this)));
}

void GapisService::OnCdmInitialized(
    content::CdmHelper::InitializeResult result) {
  CHECK(base::FeatureList::IsEnabled(kGapisSignChallenge));
  if (result != content::CdmHelper::InitializeResult::kSuccess) {
    cdm_initialization_state_ = CdmInitializationState::kFailedToInitialize;
    DVLOG(1) << "CDM initialization failed: " << static_cast<int>(result);
    return;
  }

  cdm_initialization_state_ = CdmInitializationState::kInitializedSuccessfully;
  DVLOG(1) << "CDM initialized successfully";
  FetchChallenge();
}

void GapisService::FetchChallenge() {
  CHECK_EQ(cdm_initialization_state_,
           CdmInitializationState::kInitializedSuccessfully);

  // TODO(b/485217840): fetch challenge from the server.
  const std::string challenge_init_data_hex =
      "000000377073736801000000EDEF8BA979D64ACEA3C827DCD51D21ED0000000000000013"
      "220B0A1AE899E22C0307ACA26748E3DC959B06";
  OnChallengeFetched(challenge_init_data_hex);
}

void GapisService::OnChallengeFetched(
    const std::string& challenge_init_data_hex) {
  if (challenge_init_data_hex.empty()) {
    DVLOG(1) << "Challenge is empty";
    return;
  }

  SignChallenge(challenge_init_data_hex);
}

void GapisService::SignChallenge(const std::string& challenge) {
  if (!base::FeatureList::IsEnabled(kGapisSignChallenge)) {
    CHECK(!cdm_helper_);
    // Simulate a signed challenge.
    OnChallengeSigned("signed_challenge",
                      content::CdmHelper::SignChallengeResult::kSuccess);
    return;
  }

  cdm_helper_->SignChallenge(
      challenge,
      base::BindOnce(&GapisService::OnChallengeSigned, base::Unretained(this)));
}

void GapisService::OnChallengeSigned(
    const std::string& signed_challenge,
    content::CdmHelper::SignChallengeResult result) {
  if (signed_challenge.empty()) {
    DVLOG(1) << "Signed challenge is empty, result: "
             << static_cast<int>(result);
    return;
  }

  CHECK(!ongoing_access_token_fetch_);
  ongoing_access_token_fetch_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          signin::OAuthConsumerId::kGapisService, &identity_manager_.get(),
          base::BindOnce(&GapisService::OnAccessTokenFetched,
                         base::Unretained(this), signed_challenge),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void GapisService::OnAccessTokenFetched(
    const std::string& signed_challenge,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  ongoing_access_token_fetch_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    // Access token fetch failed.
    DVLOG(1) << "GapisService access token fetch failed with error: "
             << error.state();
    return;
  }

  CHECK(!ongoing_app_token_downloader_);

  // Use the sync service URL as the base URL for the GAPIS service.
  ongoing_app_token_downloader_ =
      std::make_unique<TokenDownloader>(service_url_, url_loader_factory_);

  ongoing_app_token_downloader_->FetchToken(
      base::BindOnce(&GapisService::OnAppTokenFetched, base::Unretained(this)),
      access_token_info.token, signed_challenge);
}

void GapisService::OnAppTokenFetched(const std::string& app_token) {
  ongoing_app_token_downloader_.reset();
  CHECK(!ongoing_access_token_fetch_);
  if (app_token.empty()) {
    DVLOG(1) << "GapisService app token fetch failed";
    return;
  }

  // Override the app token in the GAPIS manager.
  // TODO(b/485217840): Add token TTL to expire tokens.
  GapisManager::GetInstance()->SetAppToken(app_token);
}

}  // namespace gapis
