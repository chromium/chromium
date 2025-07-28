// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/internal/identity_manager/oauth_consumer_registry.h"

#include "base/notreached.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {
constexpr char kSyncOAuthConsumerName[] = "sync";
constexpr char kWallpaperGooglePhotosFetcherName[] =
    "wallpaper_google_photos_fetcher";
constexpr char kWallpaperFetcherDelegateName[] = "wallpaper_fetcher_delegate";
constexpr char kIpProtectionServiceName[] = "ip_protection_service";
constexpr char kSanitizedImageSourceName[] = "sanitized_image_source";
constexpr char kOptimizationGuideGetHintsName[] =
    "optimization_guide_get_hints";
constexpr char kOptimizationGuideModelExecutionName[] =
    "optimization_guide_model_execution";
}

namespace signin {

OAuthConsumer::OAuthConsumer(const std::string& name, const ScopeSet& scopes)
    : name_(name), scopes_(scopes) {
  CHECK(!name.empty());
  CHECK(!scopes.empty());
}

OAuthConsumer::~OAuthConsumer() = default;

std::string OAuthConsumer::GetName() const {
  return name_;
}

ScopeSet OAuthConsumer::GetScopes() const {
  return scopes_;
}

OAuthConsumer GetOAuthConsumerFromId(OAuthConsumerId oauth_consumer_id) {
  switch (oauth_consumer_id) {
    case OAuthConsumerId::kSync:
      return OAuthConsumer(
          /*name=*/kSyncOAuthConsumerName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kWallpaperGooglePhotosFetcher:
      return OAuthConsumer(
          /*name=*/kWallpaperGooglePhotosFetcherName,
          /*scopes=*/{GaiaConstants::kPhotosModuleOAuth2Scope});
    case OAuthConsumerId::kWallpaperFetcherDelegate:
      return OAuthConsumer(
          /*name=*/kWallpaperFetcherDelegateName,
          /*scopes=*/{GaiaConstants::kPhotosModuleImageOAuth2Scope});
    case OAuthConsumerId::kIpProtectionService:
      return OAuthConsumer(
          /*name=*/kIpProtectionServiceName,
          /*scopes=*/{GaiaConstants::kIpProtectionAuthScope});
    case OAuthConsumerId::kSanitizedImageSource:
      return OAuthConsumer(
          /*name=*/kSanitizedImageSourceName,
          /*scopes=*/{GaiaConstants::kPhotosModuleImageOAuth2Scope});
    case OAuthConsumerId::kOptimizationGuideGetHints:
      return OAuthConsumer(
          /*name=*/kOptimizationGuideGetHintsName,
          /*scopes=*/{
              GaiaConstants::kOptimizationGuideServiceGetHintsOAuth2Scope});
    case OAuthConsumerId::kOptimizationGuideModelExecution:
      return OAuthConsumer(
          /*name=*/kOptimizationGuideModelExecutionName,
          /*scopes=*/{GaiaConstants::
                          kOptimizationGuideServiceModelExecutionOAuth2Scope});
  }
  NOTREACHED();
}

}  // namespace signin
