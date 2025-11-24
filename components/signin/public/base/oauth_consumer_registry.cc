// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/oauth_consumer_registry.h"

#include "google_apis/gaia/gaia_constants.h"

namespace {

constexpr char kSyncName[] = "sync";
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
constexpr char kExtensionDownloaderName[] = "extension_downloader";
constexpr char kEnclaveManagerName[] = "enclave_manager";
constexpr char kNtpDriveServiceName[] = "ntp_drive_service";
constexpr char kForceSigninVerifierName[] = "force_signin_verifier";
constexpr char kCaptureModeDelegateName[] = "capture_mode_delegate";
constexpr char kFcmInvalidationName[] = "fcm_invalidation";
constexpr char kNearbyShareName[] = "nearby_share";
constexpr char kAdvancedProtectionStatusManagerName[] =
    "advanced_protection_status_manager";
constexpr char kPushNotificationName[] = "push_notification";
constexpr char kKAnonymityServiceName[] = "k_anonymity_service";
constexpr char kFeedbackUploaderName[] = "feedback_uploader";
constexpr char kPasswordSharingRecipientsDownloaderName[] =
    "password_sharing_recipients_downloader";
constexpr char kWebHistoryServiceName[] = "web_history";
constexpr char kComposeboxQueryControllerName[] = "ComposeboxQueryController";
constexpr char kDocumentSuggestionsServiceName[] =
    "document_suggestions_service";
constexpr char kEnterpriseSearchAggregatorName[] =
    "enterprise_search_aggregator";
constexpr char kParentPermissionDialogName[] = "parent_permission_dialog";
constexpr char kUserCloudSigninRestrictionPolicyFetcherName[] =
    "user_cloud_signin_restriction_policy_fetcher";
constexpr char kCloudPolicyClientRegistrationName[] =
    "cloud_policy_client_registration";
constexpr char kSafeBrowsingName[] = "safe_browsing_service";
constexpr char kTailoredSecurityServiceName[] = "tailored_security_service";
constexpr char kLensOverlayQueryControllerName[] =
    "lens_overlay_query_controller";
constexpr char kTrustedVaultFrontendName[] = "trusted_vault_frontend";
constexpr char kFeedNetworkName[] = "feed_network";
constexpr char kAutofillPaymentsName[] = "autofill_payments";
constexpr char kPaymentsAccessTokenFetcherName[] =
    "payments_access_token_fetcher";
constexpr char kSaveToDriveName[] = "save_to_drive";
constexpr char kFastPairName[] = "fast_pair";
constexpr char kEduCoexistenceLoginHandlerName[] =
    "edu_coexistence_login_handler";
constexpr char kEduAccountLoginHandlerName[] = "edu_account_login_handler";
constexpr char kChromeosFamilyLinkUserMetricsProviderName[] =
    "chromeos_family_link_user_metrics_provider";
constexpr char kEnterpriseIdentityServiceName[] = "enterprise_identity_service";
constexpr char kPromotionEligibilityCheckerName[] =
    "promotion_eligibility_checker";
constexpr char kPasswordManagerLeakDetectionName[] =
    "password_manager_leak_detection";
constexpr char kAndroidManagementClientName[] = "android_management_client";
constexpr char kArcBackgroundAuthCodeFetcherName[] =
    "arc_background_auth_code_fetcher";
constexpr char kGcmAccountTrackerName[] = "gcm_account_tracker";
constexpr char kPolicyTokenForwarderName[] = "policy_token_forwarder";
constexpr char kPluginVmLicenseCheckerName[] = "plugin_vm_license_checker";
constexpr char kDrivefsAuthName[] = "drivefs_auth";
constexpr char kNearbyPresenceServerClientName[] =
    "nearby_presence_server_client";
constexpr char kCryptAuthClientName[] = "crypt_auth_client";
constexpr char kAmbientModeName[] = "ambient_mode";
constexpr char kProfileDownloaderName[] = "profile_downloader";
constexpr char kDataSharingAndroidName[] = "data_sharing_android";
constexpr char kExtensionsIdentityAPIName[] = "extensions_identity_api";
constexpr char kMantaName[] = "manta";
constexpr char kChromeMemexName[] = "chrome_memex";
constexpr char kDevtoolsAidaName[] = "devtools_aida_client";
constexpr char kChromeOsBabelOrcaName[] = "chromeos_babel_orca";
constexpr char kChromeOsBocaSchoolToolsAuthName[] =
    "chromeos_boca_school_tools_auth";
constexpr char kSharedDataPreviewName[] = "shared_data_preview";
constexpr char kAccessCodeCastDiscoveryName[] = "access_code_cast_discovery";
constexpr char kAuthServiceDriveApiName[] = "auth_service_drive_api";
constexpr char kAuthServiceCalendarName[] = "auth_service_calendar";
constexpr char kAuthServiceGlanceablesClassroomName[] =
    "auth_service_glanceables_classroom";
constexpr char kAuthServiceTasksClientName[] = "auth_service_tasks_client";
constexpr char kYouTubeMusicName[] = "youtube_music";
constexpr char kContextualTasksName[] = "contextual_tasks";

}  // namespace

namespace signin {

OAuthConsumerRegistry::OAuthConsumerRegistry() = default;
OAuthConsumerRegistry::~OAuthConsumerRegistry() = default;

OAuthConsumer OAuthConsumerRegistry::GetOAuthConsumerFromId(
    OAuthConsumerId oauth_consumer_id) const {
  switch (oauth_consumer_id) {
    case OAuthConsumerId::kSync:
      return OAuthConsumer(
          /*name=*/kSyncName,
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
    case OAuthConsumerId::kExtensionDownloader:
      return OAuthConsumer(
          /*name=*/kExtensionDownloaderName,
          /*scopes=*/{GaiaConstants::kWebstoreOAuth2Scope});
    case OAuthConsumerId::kEnclaveManager:
      return OAuthConsumer(
          /*name=*/kEnclaveManagerName,
          /*scopes=*/{GaiaConstants::kPasskeysEnclaveOAuth2Scope});
    case OAuthConsumerId::kNtpDriveService:
      return OAuthConsumer(
          /*name=*/kNtpDriveServiceName,
          /*scopes=*/{GaiaConstants::kDriveReadOnlyOAuth2Scope});
    case OAuthConsumerId::kForceSigninVerifier:
      return OAuthConsumer(
          /*name=*/kForceSigninVerifierName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kCaptureModeDelegate:
      return OAuthConsumer(
          /*name=*/kCaptureModeDelegateName,
          /*scopes=*/{GaiaConstants::kSupportContentOAuth2Scope});
    case OAuthConsumerId::kFcmInvalidation:
      return OAuthConsumer(
          /*name=*/kFcmInvalidationName,
          /*scopes=*/{GaiaConstants::kFCMOAuthScope});
    case OAuthConsumerId::kNearbyShare:
      return OAuthConsumer(
          /*name=*/kNearbyShareName,
          /*scopes=*/{GaiaConstants::kNearbyShareOAuth2Scope});
    case OAuthConsumerId::kAdvancedProtectionStatusManager:
      return OAuthConsumer(
          /*name=*/kAdvancedProtectionStatusManagerName,
          /*scopes=*/{GaiaConstants::kOAuth1LoginScope});
    case OAuthConsumerId::kPushNotification:
      return OAuthConsumer(
          /*name=*/kPushNotificationName,
          /*scopes=*/{GaiaConstants::kPushNotificationOAuth2Scope});
    case OAuthConsumerId::kKAnonymityService:
      return OAuthConsumer(
          /*name=*/kKAnonymityServiceName,
          /*scopes=*/{GaiaConstants::kKAnonymityServiceOAuth2Scope});
    case OAuthConsumerId::kFeedbackUploader:
      return OAuthConsumer(
          /*name=*/kFeedbackUploaderName,
          /*scopes=*/{GaiaConstants::kSupportContentOAuth2Scope});
    case OAuthConsumerId::kPasswordSharingRecipientsDownloader:
      return OAuthConsumer(
          /*name=*/kPasswordSharingRecipientsDownloaderName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kWebHistoryService:
      return OAuthConsumer(
          /*name=*/kWebHistoryServiceName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kComposeboxQueryController:
      return OAuthConsumer(
          /*name=*/kComposeboxQueryControllerName,
          /*scopes=*/{GaiaConstants::kLensOAuth2Scope});
    case OAuthConsumerId::kDocumentSuggestionsService:
      return OAuthConsumer(
          /*name=*/kDocumentSuggestionsServiceName,
          /*scopes=*/{GaiaConstants::kCloudSearchQueryOAuth2Scope});
    case OAuthConsumerId::kEnterpriseSearchAggregator:
      return OAuthConsumer(
          /*name=*/kEnterpriseSearchAggregatorName,
          /*scopes=*/{GaiaConstants::kDiscoveryEngineCompleteQueryOAuth2Scope});
    case OAuthConsumerId::kParentPermissionDialog:
      return OAuthConsumer(
          /*name=*/kParentPermissionDialogName,
          /*scopes=*/{GaiaConstants::kAccountsReauthOAuth2Scope});
    case OAuthConsumerId::kUserCloudSigninRestrictionPolicyFetcher:
      return OAuthConsumer(
          /*name=*/kUserCloudSigninRestrictionPolicyFetcherName,
          /*scopes=*/{GaiaConstants::kSecureConnectOAuth2Scope});
    case OAuthConsumerId::kCloudPolicyClientRegistration:
      return OAuthConsumer(
          /*name=*/kCloudPolicyClientRegistrationName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kSafeBrowsing:
      return OAuthConsumer(
          /*name=*/kSafeBrowsingName,
          /*scopes=*/{GaiaConstants::kChromeSafeBrowsingOAuth2Scope});
    case OAuthConsumerId::kTailoredSecurityService:
      return OAuthConsumer(
          /*name=*/kTailoredSecurityServiceName,
          /*scopes=*/{GaiaConstants::kChromeSafeBrowsingOAuth2Scope});
    case OAuthConsumerId::kLensOverlayQueryController:
      return OAuthConsumer(
          /*name=*/kLensOverlayQueryControllerName,
          /*scopes=*/{GaiaConstants::kLensOAuth2Scope});
    case OAuthConsumerId::kTrustedVaultFrontend:
      return OAuthConsumer(
          /*name=*/kTrustedVaultFrontendName,
          /*scopes=*/{GaiaConstants::kCryptAuthOAuth2Scope});
    case OAuthConsumerId::kFeedNetwork:
      return OAuthConsumer(
          /*name=*/kFeedNetworkName,
          /*scopes=*/{GaiaConstants::kFeedOAuth2Scope});
    case OAuthConsumerId::kAutofillPayments:
      return OAuthConsumer(
          /*name=*/kAutofillPaymentsName,
          /*scopes=*/{GaiaConstants::kPaymentsOAuth2Scope});
    case OAuthConsumerId::kPaymentsAccessTokenFetcher:
      return OAuthConsumer(
          /*name=*/kPaymentsAccessTokenFetcherName,
          /*scopes=*/{GaiaConstants::kPaymentsOAuth2Scope});
    case OAuthConsumerId::kSaveToDrive:
      return OAuthConsumer(
          /*name=*/kSaveToDriveName,
          /*scopes=*/{GaiaConstants::kDriveOAuth2Scope});
    case OAuthConsumerId::kFastPair:
      return OAuthConsumer(
          /*name=*/kFastPairName,
          /*scopes=*/{GaiaConstants::kNearbyDevicesOAuth2Scope});
    case OAuthConsumerId::kEduCoexistenceLoginHandler:
      return OAuthConsumer(
          /*name=*/kEduCoexistenceLoginHandlerName,
          /*scopes=*/{GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope,
                      GaiaConstants::kAccountsReauthOAuth2Scope,
                      GaiaConstants::kAuditRecordingOAuth2Scope,
                      GaiaConstants::kClearCutOAuth2Scope,
                      GaiaConstants::kKidManagementPrivilegedOAuth2Scope});
    case OAuthConsumerId::kEduAccountLoginHandler:
      return OAuthConsumer(
          /*name=*/kEduAccountLoginHandlerName,
          /*scopes=*/{GaiaConstants::kAccountsReauthOAuth2Scope});
    case OAuthConsumerId::kChromeosFamilyLinkUserMetricsProvider:
      return OAuthConsumer(
          /*name=*/kChromeosFamilyLinkUserMetricsProviderName,
          /*scopes=*/{});
    case OAuthConsumerId::kEnterpriseIdentityService:
      return OAuthConsumer(
          /*name=*/kEnterpriseIdentityServiceName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth});
    case OAuthConsumerId::kPromotionEligibilityChecker:
      return OAuthConsumer(
          /*name=*/kPromotionEligibilityCheckerName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kPasswordManagerLeakDetection:
      return OAuthConsumer(
          /*name=*/kPasswordManagerLeakDetectionName,
          /*scopes=*/{GaiaConstants::kPasswordsLeakCheckOAuth2Scope});
    case OAuthConsumerId::kAndroidManagementClient:
      return OAuthConsumer(
          /*name=*/kAndroidManagementClientName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kArcBackgroundAuthCodeFetcher:
      return OAuthConsumer(
          /*name=*/kArcBackgroundAuthCodeFetcherName,
          /*scopes=*/{GaiaConstants::kOAuth1LoginScope});
    case OAuthConsumerId::kGcmAccountTracker:
      return OAuthConsumer(
          /*name=*/kGcmAccountTrackerName,
          /*scopes=*/{GaiaConstants::kGCMGroupServerOAuth2Scope,
                      GaiaConstants::kGCMCheckinServerOAuth2Scope});
    case OAuthConsumerId::kPolicyTokenForwarder:
      return OAuthConsumer(
          /*name=*/kPolicyTokenForwarderName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kPluginVmLicenseChecker:
      return OAuthConsumer(
          /*name=*/kPluginVmLicenseCheckerName,
          /*scopes=*/{GaiaConstants::kLicenseCheckOAuth2Scope});
    case OAuthConsumerId::kDrivefsAuth:
      return OAuthConsumer(
          /*name=*/kDrivefsAuthName,
          /*scopes=*/{GaiaConstants::kClientChannelOAuth2Scope,
                      GaiaConstants::kDriveOAuth2Scope,
                      GaiaConstants::kExperimentsAndConfigsOAuth2Scope});
    case OAuthConsumerId::kNearbyPresenceServerClient:
      return OAuthConsumer(
          /*name=*/kNearbyPresenceServerClientName,
          /*scopes=*/{GaiaConstants::kNearbyPresenceOAuth2Scope});
    case OAuthConsumerId::kCryptAuthClient:
      return OAuthConsumer(
          /*name=*/kCryptAuthClientName,
          /*scopes=*/{GaiaConstants::kCryptAuthOAuth2Scope});
    case OAuthConsumerId::kAmbientMode:
      return OAuthConsumer(
          /*name=*/kAmbientModeName,
          /*scopes=*/{GaiaConstants::kPhotosOAuth2Scope,
                      GaiaConstants::kCastBackdropOAuth2Scope});
    case OAuthConsumerId::kProfileDownloader:
      return OAuthConsumer(
          /*name=*/kProfileDownloaderName,
          /*scopes=*/{GaiaConstants::kGoogleUserInfoProfile,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kDataSharingAndroid:
      return OAuthConsumer(
          /*name=*/kDataSharingAndroidName,
          /*scopes=*/{GaiaConstants::kPeopleApiReadWriteOAuth2Scope,
                      GaiaConstants::kPeopleApiReadOnlyOAuth2Scope});
    case OAuthConsumerId::kExtensionsIdentityAPI:
      return OAuthConsumer(
          /*name=*/kExtensionsIdentityAPIName,
          /*scopes=*/{GaiaConstants::kAnyApiOAuth2Scope});
    case OAuthConsumerId::kManta:
      return OAuthConsumer(
          /*name=*/kMantaName,
          /*scopes=*/{GaiaConstants::kMantaOAuth2Scope});
    case OAuthConsumerId::kChromeMemex:
      return OAuthConsumer(
          /*name=*/kChromeMemexName,
          /*scopes=*/{GaiaConstants::kChromeMemexOAuth2Scope});
    case OAuthConsumerId::kDevtoolsAida:
      return OAuthConsumer(
          /*name=*/kDevtoolsAidaName,
          /*scopes=*/{GaiaConstants::kAidaOAuth2Scope});
    case OAuthConsumerId::kChromeOsBabelOrca:
      return OAuthConsumer(
          /*name=*/kChromeOsBabelOrcaName,
          /*scopes=*/{GaiaConstants::kTachyonOAuthScope});
    case signin::OAuthConsumerId::kChromeOsBocaSchoolToolsAuth:
      return OAuthConsumer(
          /*name=*/kChromeOsBocaSchoolToolsAuthName,
          /*scopes=*/{GaiaConstants::kSchoolToolsAuthScope});
    case OAuthConsumerId::kSharedDataPreview:
      return OAuthConsumer(
          /*name=*/kSharedDataPreviewName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kAccessCodeCastDiscovery:
      return OAuthConsumer(
          /*name=*/kAccessCodeCastDiscoveryName,
          /*scopes=*/{GaiaConstants::kDiscoveryOAuth2Scope});
    case OAuthConsumerId::kAuthServiceDriveApi:
      return OAuthConsumer(
          /*name=*/kAuthServiceDriveApiName,
          /*scopes=*/{GaiaConstants::kDriveOAuth2Scope,
                      GaiaConstants::kDriveAppsOAuth2Scope,
                      GaiaConstants::kDriveAppsReadonlyOAuth2Scope});
    case OAuthConsumerId::kAuthServiceCalendar:
      return OAuthConsumer(
          /*name=*/kAuthServiceCalendarName,
          /*scopes=*/{GaiaConstants::kCalendarReadOnlyOAuth2Scope});
    case OAuthConsumerId::kAuthServiceGlanceablesClassroom:
      return OAuthConsumer(
          /*name=*/kAuthServiceGlanceablesClassroomName,
          /*scopes=*/{
              GaiaConstants::kClassroomReadOnlyCoursesOAuth2Scope,
              GaiaConstants::kClassroomReadOnlyCourseWorkSelfOAuth2Scope,
              GaiaConstants::
                  kClassroomReadOnlyStudentSubmissionsSelfOAuth2Scope});
    case OAuthConsumerId::kAuthServiceTasksClient:
      return OAuthConsumer(
          /*name=*/kAuthServiceTasksClientName,
          /*scopes=*/{GaiaConstants::kTasksReadOnlyOAuth2Scope,
                      GaiaConstants::kTasksOAuth2Scope});
    case OAuthConsumerId::kYouTubeMusic:
      return OAuthConsumer(
          /*name=*/kYouTubeMusicName,
          /*scopes=*/{GaiaConstants::kYouTubeMusicOAuth2Scope});
    case OAuthConsumerId::kContextualTasks:
      // TODO(crbug.com/461578148): Remove kChromeSyncOAuth2Scope once a scope
      // is created specifically for the search results page.
      return OAuthConsumer(
          /*name=*/kContextualTasksName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope,
                      GaiaConstants::kClearCutOAuth2Scope});
  }
}

}  // namespace signin
