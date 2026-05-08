// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/oauth_consumer_registry.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/contextual_tasks/public/features.h"
#include "components/wallet/core/common/wallet_features.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {

// Keep the list of OAuth2 scopes sorted alphabetically.
// keep-sorted start case=no
// OAuth2 scope for access to the Reauth flow.
constexpr char kAccountsReauthOAuth2Scope[] =
    "https://www.googleapis.com/auth/accounts.reauth";
constexpr char kAgenticPermissionOAuth2Scope[] =
    "https://www.googleapis.com/auth/agenticpermission";
// OAuth2 scope for DevTools GenAI features.
constexpr char kAiCodeOAuth2Scope[] = "https://www.googleapis.com/auth/aicode";
// OAuth2 scope for DevTools GenAI features.
constexpr char kAidaOAuth2Scope[] = "https://www.googleapis.com/auth/aida";
// OAuth2 scope for access to audit recording (ARI).
constexpr char kAuditRecordingOAuth2Scope[] =
    "https://www.googleapis.com/auth/auditrecording-pa";
// OAuth 2 scope for readonly access to Calendar.
constexpr char kCalendarReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/calendar.readonly";
// OAuth2 scope for access to Cast backdrop API.
constexpr char kCastBackdropOAuth2Scope[] =
    "https://www.googleapis.com/auth/cast.backdrop";
// OAuth2 scope to access the ChromebookEmailService API.
constexpr char kChromebookOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromebook.email";
constexpr char kChromeMemexOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromememex";
// OAuth2 scope for access to Chrome safe browsing API.
constexpr char kChromeSafeBrowsingOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-safe-browsing";
// OAuth2 scope for access to kid permissions by URL.
constexpr char kClassifyUrlKidPermissionOAuth2Scope[] =
    "https://www.googleapis.com/auth/kid.permission";
// OAuth 2 scopes for Google Classroom API.
// https://developers.google.com/identity/protocols/oauth2/scopes#classroom
constexpr char kClassroomCourseWorkMaterialsOAuthScope[] =
    "https://www.googleapis.com/auth/classroom.courseworkmaterials";
constexpr char kClassroomProfileEmailOauth2Scope[] =
    "https://www.googleapis.com/auth/classroom.profile.emails";
constexpr char kClassroomProfilePhotoUrlScope[] =
    "https://www.googleapis.com/auth/classroom.profile.photos";
constexpr char kClassroomReadOnlyCoursesOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.courses.readonly";
constexpr char kClassroomReadOnlyCourseWorkSelfOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.coursework.me.readonly";
constexpr char kClassroomReadOnlyCourseWorkStudentsOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.coursework.students.readonly";
constexpr char kClassroomReadOnlyRostersOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.rosters.readonly";
constexpr char kClassroomReadOnlyStudentSubmissionsSelfOAuth2Scope[] =
    "https://www.googleapis.com/auth/classroom.student-submissions.me.readonly";
// OAuth2 scope for access to clear cut logs.
constexpr char kClearCutOAuth2Scope[] = "https://www.googleapis.com/auth/cclog";
// OAuth2 scope for access for DriveFS to use client-side notifications.
constexpr char kClientChannelOAuth2Scope[] =
    "https://www.googleapis.com/auth/client_channel";
// OAuth2 scope for Cloud Search query API.
constexpr char kCloudSearchQueryOAuth2Scope[] =
    "https://www.googleapis.com/auth/cloud_search.query";
// OAuth2 scope for read-write access to contacts.
constexpr char kContactsOAuth2Scope[] =
    "https://www.googleapis.com/auth/contacts";
constexpr char kCryptAuthOAuth2Scope[] =
    "https://www.googleapis.com/auth/cryptauth";
// OAuth2 scope for Discovery Engine suggestion API.
constexpr char kDiscoveryEngineCompleteQueryOAuth2Scope[] =
    "https://www.googleapis.com/auth/discoveryengine.complete_query";
// OAuth2 scope for Access Code Cast.
constexpr char kDiscoveryOAuth2Scope[] =
    "https://www.googleapis.com/auth/cast-edu-messaging";
// OAuth2 scope for access to Drive Apps.
constexpr char kDriveAppsOAuth2Scope[] =
    "https://www.googleapis.com/auth/drive.apps";
// OAuth2 scope for access to readonly Drive Apps.
constexpr char kDriveAppsReadonlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/drive.apps.readonly";
// OAuth 2 scope for readonly access to Drive.
constexpr char kDriveReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/drive.readonly";
// OAuth2 scope for access for DriveFS to access flags.
constexpr char kExperimentsAndConfigsOAuth2Scope[] =
    "https://www.googleapis.com/auth/experimentsandconfigs";
// OAuth 2 scope for the Discover feed.
constexpr char kFeedOAuth2Scope[] = "https://www.googleapis.com/auth/googlenow";
// OAuth2 scopes for access to GCM.
constexpr char kGCMCheckinServerOAuth2Scope[] =
    "https://www.googleapis.com/auth/android_checkin";
constexpr char kGCMGroupServerOAuth2Scope[] =
    "https://www.googleapis.com/auth/gcm";
// OAuth2 scope for DevTools Google Developer Program features.
constexpr char kGdpOAuth2Scope[] =
    "https://www.googleapis.com/auth/devprofiles.full_control";
// OAuth2 scope for readonly access to Gmail metadata.
constexpr char kGmailMetadataOAuth2Scope[] =
    "https://www.googleapis.com/auth/gmail.metadata";
// OAuth2 scope for readonly access to Gmail OTP email data.
constexpr char kGmailOtpReadonlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/gmail.otp.readonly";
// OAuth 2 scope for the k-Anonymity Service API.
constexpr char kKAnonymityServiceOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromekanonymity";
// OAuth2 scope for access to kid family (read-only).
constexpr char kKidFamilyReadonlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/kid.family.readonly";
// OAuth2 scope for parental consent logging for secondary account addition.
constexpr char kKidManagementPrivilegedOAuth2Scope[] =
    "https://www.googleapis.com/auth/kid.management.privileged";
// OAuth2 scope for access to Google Family Link Supervision Setup.
constexpr char kKidsSupervisionSetupChildOAuth2Scope[] =
    "https://www.googleapis.com/auth/kids.supervision.setup.child";
// OAuth2 scope for Lens.
constexpr char kLensOAuth2Scope[] = "https://www.googleapis.com/auth/lens";
// OAuth2 scope for app license check.
constexpr char kLicenseCheckOAuth2Scope[] =
    "https://www.googleapis.com/auth/applicense.bytebot";
// OAuth2 scope for manta.
constexpr char kMantaOAuth2Scope[] =
    "https://www.googleapis.com/auth/mdi.aratea";
// OAuth2 scope for access to nearby devices (fast pair) APIs.
constexpr char kNearbyDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbydevices-pa";
// OAuth2 scope for access to nearby sharing.
constexpr char kNearbyPresenceOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbypresence-pa";
// OAuth2 scope for access to nearby sharing.
constexpr char kNearbyShareOAuth2Scope[] =
    "https://www.googleapis.com/auth/nearbysharing-pa";
// OAuth2 scope for One Time Token Service.
constexpr char kOneTimeTokenOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome.passwords.onetimetoken";
// OAuth2 scopes for Optimization Guide.
constexpr char kOptimizationGuideServiceGetHintsOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-optimization-guide";
constexpr char kOptimizationGuideServiceModelExecutionOAuth2Scope[] =
    "https://www.googleapis.com/auth/chrome-model-execution";
// OAuth2 scope for access to the parent approval widget.
constexpr char kParentApprovalOAuth2Scope[] =
    "https://www.googleapis.com/auth/kids.parentapproval";
// OAuth 2 scope for Google Password Manager passkey enclaves.
constexpr char kPasskeysEnclaveOAuth2Scope[] =
    "https://www.googleapis.com/auth/secureidentity.action";
// OAuth2 scope for access to passwords leak checking API.
constexpr char kPasswordsLeakCheckOAuth2Scope[] =
    "https://www.googleapis.com/auth/identity.passwords.leak.check";
// OAuth2 scope for access to payments.
constexpr char kPaymentsOAuth2Scope[] =
    "https://www.googleapis.com/auth/wallet.chrome";
// OAuth2 scope for access to the people API (read-only).
constexpr char kPeopleApiReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/peopleapi.readonly";
// OAuth2 scope for access to the people API (read-write).
constexpr char kPeopleApiReadWriteOAuth2Scope[] =
    "https://www.googleapis.com/auth/peopleapi.readwrite";
// OAuth 2 scope for NTP Photos module image API.
constexpr char kPhotosModuleImageOAuth2Scope[] =
    "https://www.googleapis.com/auth/photos.image.readonly";
// OAuth 2 scope for NTP Photos module API.
constexpr char kPhotosModuleOAuth2Scope[] =
    "https://www.googleapis.com/auth/photos.firstparty.readonly";
// OAuth2 scope for access to the Photos API.
constexpr char kPhotosOAuth2Scope[] = "https://www.googleapis.com/auth/photos";
// OAuth2 scope for Private AI.
constexpr char kPrivateAiAuthScope[] = "https://www.googleapis.com/auth/paic";
// OAuth2 scope for access to the people API person's locale preferences
// (read-only).
constexpr char kProfileLanguageReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/profile.language.read";
// OAuth2 scope for access to the programmatic challenge API (read-only).
constexpr char kProgrammaticChallengeOAuth2Scope[] =
    "https://www.googleapis.com/auth/accounts.programmaticchallenge";
// OAuth2 scope for push notifications.
constexpr char kPushNotificationOAuth2Scope[] =
    "https://www.googleapis.com/auth/notifications";
constexpr char kSchoolToolsAuthScope[] =
    "https://www.googleapis.com/auth/chromeosschooltools";
constexpr char kSearchResultsOAuth2Scope[] =
    "https://www.googleapis.com/auth/searchresults";
// OAuth2 scope for Site Automation Index.
constexpr char kSiteAutomationIndexOAuth2Scope[] =
    "https://www.googleapis.com/auth/siteautomationindex";
// OAuth2 scope for access to Tachyon api.
constexpr char kTachyonOAuthScope[] = "https://www.googleapis.com/auth/tachyon";
// OAuth 2 scopes for Google Tasks API.
// https://developers.google.com/identity/protocols/oauth2/scopes#tasks
constexpr char kTasksOAuth2Scope[] = "https://www.googleapis.com/auth/tasks";
constexpr char kTasksReadOnlyOAuth2Scope[] =
    "https://www.googleapis.com/auth/tasks.readonly";
constexpr char kWalletPassesOAuth2Scope[] =
    "https://www.googleapis.com/auth/wallet_1p_passes";
// OAuth2 scope for web history.
constexpr char kWebHistoryOAuth2Scope[] =
    "https://www.googleapis.com/auth/webhistory";
// OAuth2 scope for Chrome Web Store.
constexpr char kWebstoreOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromewebstore.readonly";
// OAuth 2 scope for YouTube Music API.
// https://developers.google.com/youtube/mediaconnect/guides/authentication#identify-access-scope
constexpr char kYouTubeMusicOAuth2Scope[] =
    "https://www.googleapis.com/auth/music";
// keep-sorted end

constexpr char kSyncName[] = "sync";
constexpr char kSecureGatewayServiceName[] = "secure_gateway_service";
constexpr char kWallpaperGooglePhotosFetcherName[] =
    "wallpaper_google_photos_fetcher";
constexpr char kWallpaperFetcherDelegateName[] = "wallpaper_fetcher_delegate";
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
constexpr char kDevtoolsAiCodeName[] = "devtools_aicode_client";
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
constexpr char kDevtoolsGdpName[] = "devtools_gdp_client";
constexpr char kAshDriveIntegrationName[] = "ash_drive_integration";
constexpr char kAshClassroomPageHandlerName[] = "ash_classroom_page_handler";
constexpr char kAshScannerKeyedServiceName[] = "ash_scanner_keyed_service";
constexpr char kAshAutotestPrivateApiName[] = "ash_autotest_private_api";
constexpr char kSyncDeviceStatisticsMetricsName[] =
    "sync_device_statistics_metrics";
constexpr char kPrivateAiServiceName[] = "private_ai_service";
constexpr char kWalletPassesName[] = "wallet_passes";
constexpr char kAimEligibilityServiceName[] = "aim_eligibility_service";
constexpr char kAccessibilityAnnotatorName[] = "accessibility_annotator";
constexpr char kActorLoginPermissionServiceName[] =
    "actor_login_permission_service";
constexpr char kGapisServiceName[] = "gapis_service";
constexpr char kOneTimeTokenServiceName[] = "one_time_token_service";
constexpr char kMultistepFilterName[] = "multistep_filter";
}  // namespace

namespace signin {

BASE_FEATURE(kWebHistoryUseSpecificScope, base::FEATURE_ENABLED_BY_DEFAULT);

OAuthConsumer GetOAuthConsumerForDynamicScopes(
    OAuthConsumerId oauth_consumer_id,
    const signin::ScopeSet& scopes) {
  switch (oauth_consumer_id) {
    case OAuthConsumerId::kAshAutotestPrivateApi:
      return OAuthConsumer(kAshAutotestPrivateApiName, scopes);
    default:
      NOTREACHED();
  }
}

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
          /*scopes=*/{kPhotosModuleOAuth2Scope});
    case OAuthConsumerId::kWallpaperFetcherDelegate:
      return OAuthConsumer(
          /*name=*/kWallpaperFetcherDelegateName,
          /*scopes=*/{kPhotosModuleImageOAuth2Scope});
    case OAuthConsumerId::kSanitizedImageSource:
      return OAuthConsumer(
          /*name=*/kSanitizedImageSourceName,
          /*scopes=*/{kPhotosModuleImageOAuth2Scope});
    case OAuthConsumerId::kOptimizationGuideGetHints:
      return OAuthConsumer(
          /*name=*/kOptimizationGuideGetHintsName,
          /*scopes=*/{kOptimizationGuideServiceGetHintsOAuth2Scope});
    case OAuthConsumerId::kOptimizationGuideModelExecution:
      return OAuthConsumer(
          /*name=*/kOptimizationGuideModelExecutionName,
          /*scopes=*/{kOptimizationGuideServiceModelExecutionOAuth2Scope});
    case OAuthConsumerId::kNearbySharing:
      return OAuthConsumer(
          /*name=*/kNearbySharingName,
          /*scopes=*/{kTachyonOAuthScope});
    case OAuthConsumerId::kProjectorTokenFetcher:
      return OAuthConsumer(
          /*name=*/kProjectorTokenFetcherName,
          /*scopes=*/{GaiaConstants::kDriveOAuth2Scope,
                      kDriveReadOnlyOAuth2Scope});
    case OAuthConsumerId::kAddSupervision:
      return OAuthConsumer(
          /*name=*/kAddSupervisionName,
          /*scopes=*/{kKidsSupervisionSetupChildOAuth2Scope,
                      kPeopleApiReadOnlyOAuth2Scope, kAccountsReauthOAuth2Scope,
                      kAuditRecordingOAuth2Scope, kClearCutOAuth2Scope});
    case OAuthConsumerId::kParentAccess:
      return OAuthConsumer(
          /*name=*/kParentAccessName,
          /*scopes=*/{kParentApprovalOAuth2Scope,
                      kProgrammaticChallengeOAuth2Scope});
    case OAuthConsumerId::kDataSharing:
      return OAuthConsumer(
          /*name=*/kDataSharingName,
          /*scopes=*/{kPeopleApiReadWriteOAuth2Scope,
                      kPeopleApiReadOnlyOAuth2Scope, kClearCutOAuth2Scope});
    case OAuthConsumerId::kLauncherItemSuggest:
      return OAuthConsumer(
          /*name=*/kLauncherItemSuggestName,
          /*scopes=*/{kDriveReadOnlyOAuth2Scope});
    case OAuthConsumerId::kMarketingBackendConnector:
      return OAuthConsumer(
          /*name=*/kMarketingBackendConnectorName,
          /*scopes=*/{kChromebookOAuth2Scope});
    case OAuthConsumerId::kPasswordSyncTokenFetcher:
      return OAuthConsumer(
          /*name=*/kPasswordSyncTokenFetcherName,
          /*scopes=*/{GaiaConstants::kGoogleUserInfoEmail,
                      GaiaConstants::kDeviceManagementServiceOAuth});
    case OAuthConsumerId::kLocaleSwitchScreen:
      return OAuthConsumer(
          /*name=*/kLocaleSwitchScreenName,
          /*scopes=*/{kPeopleApiReadOnlyOAuth2Scope,
                      GaiaConstants::kGoogleUserInfoProfile,
                      kProfileLanguageReadOnlyOAuth2Scope});
    case OAuthConsumerId::kTokenHandleService:
      return OAuthConsumer(
          /*name=*/kTokenHandleServiceName,
          /*scopes=*/{GaiaConstants::kOAuth1LoginScope});
    case OAuthConsumerId::kSupervisedUserListFamilyMembers:
      return OAuthConsumer(
          /*name=*/kSupervisedUserListFamilyMembersName,
          /*scopes=*/{kKidFamilyReadonlyOAuth2Scope});
    case OAuthConsumerId::kSupervisedUserClassifyUrl:
      return OAuthConsumer(
          /*name=*/kSupervisedUserClassifyUrlName,
          /*scopes=*/{kClassifyUrlKidPermissionOAuth2Scope});
    case OAuthConsumerId::kSupervisedUserCreatePermissionRequest:
      return OAuthConsumer(
          /*name=*/kSupervisedUserCreatePermissionRequestName,
          /*scopes=*/{kClassifyUrlKidPermissionOAuth2Scope});
    case OAuthConsumerId::kExtensionDownloader:
      return OAuthConsumer(
          /*name=*/kExtensionDownloaderName,
          /*scopes=*/{kWebstoreOAuth2Scope});
    case OAuthConsumerId::kEnclaveManager:
      return OAuthConsumer(
          /*name=*/kEnclaveManagerName,
          /*scopes=*/{kPasskeysEnclaveOAuth2Scope});
    case OAuthConsumerId::kNtpDriveService:
      return OAuthConsumer(
          /*name=*/kNtpDriveServiceName,
          /*scopes=*/{kDriveReadOnlyOAuth2Scope});
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
          /*scopes=*/{kNearbyShareOAuth2Scope});
    case OAuthConsumerId::kAdvancedProtectionStatusManager:
      return OAuthConsumer(
          /*name=*/kAdvancedProtectionStatusManagerName,
          /*scopes=*/{GaiaConstants::kOAuth1LoginScope});
    case OAuthConsumerId::kPushNotification:
      return OAuthConsumer(
          /*name=*/kPushNotificationName,
          /*scopes=*/{kPushNotificationOAuth2Scope});
    case OAuthConsumerId::kKAnonymityService:
      return OAuthConsumer(
          /*name=*/kKAnonymityServiceName,
          /*scopes=*/{kKAnonymityServiceOAuth2Scope});
    case OAuthConsumerId::kFeedbackUploader:
      return OAuthConsumer(
          /*name=*/kFeedbackUploaderName,
          /*scopes=*/{GaiaConstants::kSupportContentOAuth2Scope});
    case OAuthConsumerId::kPasswordSharingRecipientsDownloader:
      return OAuthConsumer(
          /*name=*/kPasswordSharingRecipientsDownloaderName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kWebHistoryService:
      if (base::FeatureList::IsEnabled(kWebHistoryUseSpecificScope)) {
        return OAuthConsumer(
            /*name=*/kWebHistoryServiceName,
            /*scopes=*/{kWebHistoryOAuth2Scope});
      } else {
        return OAuthConsumer(
            /*name=*/kWebHistoryServiceName,
            /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
      }
    case OAuthConsumerId::kComposeboxQueryController:
      return OAuthConsumer(
          /*name=*/kComposeboxQueryControllerName,
          /*scopes=*/{kLensOAuth2Scope});
    case OAuthConsumerId::kDocumentSuggestionsService:
      return OAuthConsumer(
          /*name=*/kDocumentSuggestionsServiceName,
          /*scopes=*/{kCloudSearchQueryOAuth2Scope});
    case OAuthConsumerId::kEnterpriseSearchAggregator:
      return OAuthConsumer(
          /*name=*/kEnterpriseSearchAggregatorName,
          /*scopes=*/{kDiscoveryEngineCompleteQueryOAuth2Scope});
    case OAuthConsumerId::kParentPermissionDialog:
      return OAuthConsumer(
          /*name=*/kParentPermissionDialogName,
          /*scopes=*/{kAccountsReauthOAuth2Scope});
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
          /*scopes=*/{kChromeSafeBrowsingOAuth2Scope});
    case OAuthConsumerId::kTailoredSecurityService:
      return OAuthConsumer(
          /*name=*/kTailoredSecurityServiceName,
          /*scopes=*/{kChromeSafeBrowsingOAuth2Scope});
    case OAuthConsumerId::kLensOverlayQueryController:
      return OAuthConsumer(
          /*name=*/kLensOverlayQueryControllerName,
          /*scopes=*/{kLensOAuth2Scope});
    case OAuthConsumerId::kTrustedVaultFrontend:
      return OAuthConsumer(
          /*name=*/kTrustedVaultFrontendName,
          /*scopes=*/{kCryptAuthOAuth2Scope});
    case OAuthConsumerId::kFeedNetwork:
      return OAuthConsumer(
          /*name=*/kFeedNetworkName,
          /*scopes=*/{kFeedOAuth2Scope});
    case OAuthConsumerId::kAutofillPayments:
      return OAuthConsumer(
          /*name=*/kAutofillPaymentsName,
          /*scopes=*/{kPaymentsOAuth2Scope});
    case OAuthConsumerId::kPaymentsAccessTokenFetcher:
      return OAuthConsumer(
          /*name=*/kPaymentsAccessTokenFetcherName,
          /*scopes=*/{kPaymentsOAuth2Scope});
    case OAuthConsumerId::kSaveToDrive:
      return OAuthConsumer(
          /*name=*/kSaveToDriveName,
          /*scopes=*/{GaiaConstants::kDriveOAuth2Scope});
    case OAuthConsumerId::kFastPair:
      return OAuthConsumer(
          /*name=*/kFastPairName,
          /*scopes=*/{kNearbyDevicesOAuth2Scope});
    case OAuthConsumerId::kEduCoexistenceLoginHandler:
      return OAuthConsumer(
          /*name=*/kEduCoexistenceLoginHandlerName,
          /*scopes=*/{kKidsSupervisionSetupChildOAuth2Scope,
                      kAccountsReauthOAuth2Scope, kAuditRecordingOAuth2Scope,
                      kClearCutOAuth2Scope,
                      kKidManagementPrivilegedOAuth2Scope});
    case OAuthConsumerId::kEduAccountLoginHandler:
      return OAuthConsumer(
          /*name=*/kEduAccountLoginHandlerName,
          /*scopes=*/{kAccountsReauthOAuth2Scope});
    case OAuthConsumerId::kChromeosFamilyLinkUserMetricsProvider:
      return OAuthConsumer(
          /*name=*/kChromeosFamilyLinkUserMetricsProviderName,
          /*scopes=*/{});
    case OAuthConsumerId::kPromotionEligibilityChecker:
      return OAuthConsumer(
          /*name=*/kPromotionEligibilityCheckerName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kPasswordManagerLeakDetection:
      return OAuthConsumer(
          /*name=*/kPasswordManagerLeakDetectionName,
          /*scopes=*/{kPasswordsLeakCheckOAuth2Scope});
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
          /*scopes=*/{kGCMGroupServerOAuth2Scope,
                      kGCMCheckinServerOAuth2Scope});
    case OAuthConsumerId::kPolicyTokenForwarder:
      return OAuthConsumer(
          /*name=*/kPolicyTokenForwarderName,
          /*scopes=*/{GaiaConstants::kDeviceManagementServiceOAuth,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kPluginVmLicenseChecker:
      return OAuthConsumer(
          /*name=*/kPluginVmLicenseCheckerName,
          /*scopes=*/{kLicenseCheckOAuth2Scope});
    case OAuthConsumerId::kDrivefsAuth:
      return OAuthConsumer(
          /*name=*/kDrivefsAuthName,
          /*scopes=*/{kClientChannelOAuth2Scope,
                      GaiaConstants::kDriveOAuth2Scope,
                      kExperimentsAndConfigsOAuth2Scope});
    case OAuthConsumerId::kNearbyPresenceServerClient:
      return OAuthConsumer(
          /*name=*/kNearbyPresenceServerClientName,
          /*scopes=*/{kNearbyPresenceOAuth2Scope});
    case OAuthConsumerId::kCryptAuthClient:
      return OAuthConsumer(
          /*name=*/kCryptAuthClientName,
          /*scopes=*/{kCryptAuthOAuth2Scope});
    case OAuthConsumerId::kAmbientMode:
      return OAuthConsumer(
          /*name=*/kAmbientModeName,
          /*scopes=*/{kPhotosOAuth2Scope, kCastBackdropOAuth2Scope});
    case OAuthConsumerId::kProfileDownloader:
      return OAuthConsumer(
          /*name=*/kProfileDownloaderName,
          /*scopes=*/{GaiaConstants::kGoogleUserInfoProfile,
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kDataSharingAndroid:
      return OAuthConsumer(
          /*name=*/kDataSharingAndroidName,
          /*scopes=*/{kPeopleApiReadWriteOAuth2Scope,
                      kPeopleApiReadOnlyOAuth2Scope});
    case OAuthConsumerId::kExtensionsIdentityAPI:
      return OAuthConsumer(
          /*name=*/kExtensionsIdentityAPIName,
          /*scopes=*/{GaiaConstants::kAnyApiOAuth2Scope});
    case OAuthConsumerId::kManta:
      return OAuthConsumer(
          /*name=*/kMantaName,
          /*scopes=*/{kMantaOAuth2Scope});
    case OAuthConsumerId::kChromeMemex:
      return OAuthConsumer(
          /*name=*/kChromeMemexName,
          /*scopes=*/{kChromeMemexOAuth2Scope});
    case OAuthConsumerId::kDevtoolsAida:
      return OAuthConsumer(
          /*name=*/kDevtoolsAidaName,
          /*scopes=*/{kAidaOAuth2Scope});
    case OAuthConsumerId::kChromeOsBabelOrca:
      return OAuthConsumer(
          /*name=*/kChromeOsBabelOrcaName,
          /*scopes=*/{kTachyonOAuthScope});
    case signin::OAuthConsumerId::kChromeOsBocaSchoolToolsAuth:
      return OAuthConsumer(
          /*name=*/kChromeOsBocaSchoolToolsAuthName,
          /*scopes=*/{kSchoolToolsAuthScope});
    case OAuthConsumerId::kSharedDataPreview:
      return OAuthConsumer(
          /*name=*/kSharedDataPreviewName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kAccessCodeCastDiscovery:
      return OAuthConsumer(
          /*name=*/kAccessCodeCastDiscoveryName,
          /*scopes=*/{kDiscoveryOAuth2Scope});
    case OAuthConsumerId::kAuthServiceDriveApi:
      return OAuthConsumer(
          /*name=*/kAuthServiceDriveApiName,
          /*scopes=*/{GaiaConstants::kDriveOAuth2Scope, kDriveAppsOAuth2Scope,
                      kDriveAppsReadonlyOAuth2Scope});
    case OAuthConsumerId::kAuthServiceCalendar:
      return OAuthConsumer(
          /*name=*/kAuthServiceCalendarName,
          /*scopes=*/{kCalendarReadOnlyOAuth2Scope});
    case OAuthConsumerId::kAuthServiceGlanceablesClassroom:
      return OAuthConsumer(
          /*name=*/kAuthServiceGlanceablesClassroomName,
          /*scopes=*/{kClassroomReadOnlyCoursesOAuth2Scope,
                      kClassroomReadOnlyCourseWorkSelfOAuth2Scope,
                      kClassroomReadOnlyStudentSubmissionsSelfOAuth2Scope});
    case OAuthConsumerId::kAuthServiceTasksClient:
      return OAuthConsumer(
          /*name=*/kAuthServiceTasksClientName,
          /*scopes=*/{kTasksReadOnlyOAuth2Scope, kTasksOAuth2Scope});
    case OAuthConsumerId::kYouTubeMusic:
      return OAuthConsumer(
          /*name=*/kYouTubeMusicName,
          /*scopes=*/{kYouTubeMusicOAuth2Scope});
    case OAuthConsumerId::kContextualTasks:
      return OAuthConsumer(
          /*name=*/kContextualTasksName,
          /*scopes=*/{contextual_tasks::ShouldUseSearchResultsScope()
                          ? kSearchResultsOAuth2Scope
                          : GaiaConstants::kChromeSyncOAuth2Scope,
                      kClearCutOAuth2Scope, kLensOAuth2Scope});
    case OAuthConsumerId::kEnterprisePlusAddress:
      return GetOAuthConsumerForEnterprisePlusAddress();
    case OAuthConsumerId::kGlicUserStatus:
      return GetOAuthConsumerForGlicUserStatus();
    case OAuthConsumerId::kIndigo:
      return GetOAuthConsumerForIndigo();
    case OAuthConsumerId::kDevtoolsGdp:
      return OAuthConsumer(
          /*name=*/kDevtoolsGdpName,
          /*scopes=*/{kGdpOAuth2Scope});
    case OAuthConsumerId::kAshDriveIntegration:
      return OAuthConsumer(
          /*name=*/kAshDriveIntegrationName,
          /*scopes=*/{kDriveReadOnlyOAuth2Scope});
    case OAuthConsumerId::kAshBocaClassroomPageHandler:
      return OAuthConsumer(
          /*name=*/kAshClassroomPageHandlerName,
          /*scopes=*/{kClassroomReadOnlyRostersOAuth2Scope,
                      kClassroomReadOnlyCoursesOAuth2Scope,
                      kClassroomReadOnlyCourseWorkStudentsOAuth2Scope,
                      kClassroomProfileEmailOauth2Scope,
                      kClassroomProfilePhotoUrlScope,
                      kClassroomCourseWorkMaterialsOAuthScope});
    case OAuthConsumerId::kAshScannerKeyedService:
      return OAuthConsumer(
          /*name=*/kAshScannerKeyedServiceName,
          /*scopes=*/{kContactsOAuth2Scope});
    case OAuthConsumerId::kAshAutotestPrivateApi:
      // This consumer id should be converted using
      // GetOAuthConsumerForDynamicScopes().
      NOTREACHED();
    case OAuthConsumerId::kSyncDeviceStatisticsMetrics:
      return OAuthConsumer(
          /*name=*/kSyncDeviceStatisticsMetricsName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kPrivateAiService:
      return OAuthConsumer(
          /*name=*/kPrivateAiServiceName,
          /*scopes=*/{kPrivateAiAuthScope});
    case OAuthConsumerId::kWalletPasses: {
      CHECK(base::FeatureList::IsEnabled(
          wallet::features::kWalletApiPrivatePassesEnabled));
      return signin::OAuthConsumer(
          /*name=*/kWalletPassesName,
          /*scopes=*/{kWalletPassesOAuth2Scope});
    }
    case OAuthConsumerId::kAimEligibilityService:
      return OAuthConsumer(
          /*name=*/kAimEligibilityServiceName,
          /*scopes=*/{kSearchResultsOAuth2Scope});
    case OAuthConsumerId::kDevtoolsAiCode:
      return OAuthConsumer(
          /*name=*/kDevtoolsAiCodeName,
          /*scopes=*/{kAiCodeOAuth2Scope});
    case OAuthConsumerId::kAccessibilityAnnotator:
      // TODO(b/493530228): Use narrow scope for the accessibility annotator.
      return OAuthConsumer(
          /*name=*/kAccessibilityAnnotatorName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kActorLoginPermissionService:
      return OAuthConsumer(
          /*name=*/kActorLoginPermissionServiceName,
          /*scopes=*/{kAgenticPermissionOAuth2Scope});
    case OAuthConsumerId::kGapisService:
      return OAuthConsumer(
          /*name=*/kGapisServiceName,
          /*scopes=*/{GaiaConstants::kChromeSyncOAuth2Scope});
    case OAuthConsumerId::kOneTimeTokenService:
      return OAuthConsumer(
          /*name=*/kOneTimeTokenServiceName,
          /*scopes=*/{kOneTimeTokenOAuth2Scope, kGmailMetadataOAuth2Scope,
                      kGmailOtpReadonlyOAuth2Scope,
                      // TODO(b/506950478): Remove kGoogleUserInfoEmail scope
                      // once the service accepts kOneTimeTokenOAuth2Scope.
                      GaiaConstants::kGoogleUserInfoEmail});
    case OAuthConsumerId::kMultistepFilter:
      return OAuthConsumer(
          /*name=*/kMultistepFilterName,
          /*scopes=*/{kSiteAutomationIndexOAuth2Scope});
    case OAuthConsumerId::kGlicInvokeApi:
      return GetOAuthConsumerForGlicInvokeApi();
    case OAuthConsumerId::kSecureGatewayService:
      return OAuthConsumer(
          /*name=*/kSecureGatewayServiceName,
          /*scopes=*/{GaiaConstants::kSecureGatewayOAuth2Scope});
  }
}

}  // namespace signin
