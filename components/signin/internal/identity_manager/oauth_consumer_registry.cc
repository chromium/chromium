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
constexpr char kNearbySharingName[] = "nearby_sharing";
constexpr char kProjectorTokenFetcherName[] = "projector_token_fetcher";
constexpr char kAddSupervisionName[] = "add_supervision";
constexpr char kParentAccessName[] = "parent_access";
constexpr char kDataSharingName[] = "data_sharing";
constexpr char kLauncherItemSuggestName[] = "launcher_item_suggest";
constexpr char kMarketingBackendConnectorName[] = "marketing_backend_connector";
constexpr char kPasswordSyncTokenFetcherName[] = "password_sync_token_fetcher";
constexpr char kLocaleSwitchScreenName[] = "locale_switch_screen";
constexpr char kTokenHandleServiceName[] = "token_handle_service";
constexpr char kSupervisedUserListFamilyMembersName[] =
    "supervised_user_list_family_members";
constexpr char kSupervisedUserClassifyUrlName[] =
    "supervised_user_classify_url";
constexpr char kSupervisedUserCreatePermissionRequestName[] =
    "supervised_user_create_permission_request";

}  // namespace

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
    case OAuthConsumerId::kNearbySharing:
      return OAuthConsumer(
          /*name=*/kNearbySharingName,
          /*scopes=*/{GaiaConstants::kTachyonOAuthScope});
    case OAuthConsumerId::kProjectorTokenFetcher:
      return OAuthConsumer(
          /*name=*/kProjectorTokenFetcherName,
          /*scopes=*/{GaiaConstants::kDriveOAuth2Scope,
                      GaiaConstants::kDriveReadOnlyOAuth2Scope});
    case OAuthConsumerId::kAddSupervision:
      return OAuthConsumer(
          /*name=*/kAddSupervisionName,
          /*scopes=*/{GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope,
                      GaiaConstants::kPeopleApiReadOnlyOAuth2Scope,
                      GaiaConstants::kAccountsReauthOAuth2Scope,
                      GaiaConstants::kAuditRecordingOAuth2Scope,
                      GaiaConstants::kClearCutOAuth2Scope});
    case OAuthConsumerId::kParentAccess:
      return OAuthConsumer(
          /*name=*/kParentAccessName,
          /*scopes=*/{GaiaConstants::kParentApprovalOAuth2Scope,
                      GaiaConstants::kProgrammaticChallengeOAuth2Scope});
    case OAuthConsumerId::kDataSharing:
      return OAuthConsumer(
          /*name=*/kDataSharingName,
          /*scopes=*/{GaiaConstants::kPeopleApiReadWriteOAuth2Scope,
                      GaiaConstants::kPeopleApiReadOnlyOAuth2Scope,
                      GaiaConstants::kClearCutOAuth2Scope});
    case OAuthConsumerId::kLauncherItemSuggest:
      return OAuthConsumer(
          /*name=*/kLauncherItemSuggestName,
          /*scopes=*/{GaiaConstants::kDriveReadOnlyOAuth2Scope});
    case OAuthConsumerId::kMarketingBackendConnector:
      return OAuthConsumer(
          /*name=*/kMarketingBackendConnectorName,
          /*scopes=*/{GaiaConstants::kChromebookOAuth2Scope});
    case OAuthConsumerId::kPasswordSyncTokenFetcher:
      return OAuthConsumer(
          /*name=*/kPasswordSyncTokenFetcherName,
          /*scopes=*/{GaiaConstants::kGoogleUserInfoEmail,
                      GaiaConstants::kDeviceManagementServiceOAuth});
    case OAuthConsumerId::kLocaleSwitchScreen:
      return OAuthConsumer(
          /*name=*/kLocaleSwitchScreenName,
          /*scopes=*/{GaiaConstants::kPeopleApiReadOnlyOAuth2Scope,
                      GaiaConstants::kGoogleUserInfoProfile,
                      GaiaConstants::kProfileLanguageReadOnlyOAuth2Scope});
    case OAuthConsumerId::kTokenHandleService:
      return OAuthConsumer(
          /*name=*/kTokenHandleServiceName,
          /*scopes=*/{GaiaConstants::kOAuth1LoginScope});
    case OAuthConsumerId::kSupervisedUserListFamilyMembers:
      return OAuthConsumer(
          /*name=*/kSupervisedUserListFamilyMembersName,
          /*scopes=*/{GaiaConstants::kKidFamilyReadonlyOAuth2Scope});
    case OAuthConsumerId::kSupervisedUserClassifyUrl:
      return OAuthConsumer(
          /*name=*/kSupervisedUserClassifyUrlName,
          /*scopes=*/{GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope});
    case OAuthConsumerId::kSupervisedUserCreatePermissionRequest:
      return OAuthConsumer(
          /*name=*/kSupervisedUserCreatePermissionRequestName,
          /*scopes=*/{GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope});
  }
  NOTREACHED();
}

}  // namespace signin
