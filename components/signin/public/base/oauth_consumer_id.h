// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_ID_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_ID_H_

namespace signin {

// LINT.IfChange(OAuthConsumerId)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OAuthConsumerId {
  kSync = 0,
  kWallpaperGooglePhotosFetcher = 1,
  kWallpaperFetcherDelegate = 2,
  kIpProtectionService = 3,
  kSanitizedImageSource = 4,
  kOptimizationGuideGetHints = 5,
  kOptimizationGuideModelExecution = 6,
  kNearbySharing = 7,
  kProjectorTokenFetcher = 8,
  kAddSupervision = 9,
  kParentAccess = 10,
  kDataSharing = 11,
  kLauncherItemSuggest = 12,
  kMarketingBackendConnector = 13,
  kPasswordSyncTokenFetcher = 14,
  kLocaleSwitchScreen = 15,
  kTokenHandleService = 16,
  kSupervisedUserListFamilyMembers = 17,
  kSupervisedUserClassifyUrl = 18,
  kSupervisedUserCreatePermissionRequest = 19,
  kExtensionDownloader = 20,
  kEnclaveManager = 21,
  kNtpDriveService = 22,
  kForceSigninVerifier = 23,
  kCaptureModeDelegate = 24,
  // TODO(crbug.com/434619290): Remove this once not used anymore.
  kFcmInvalidation = 25,
  kNearbyShare = 26,
  kAdvancedProtectionStatusManager = 27,
  kPushNotification = 28,
  kKAnonymityService = 29,
  kFeedbackUploader = 30,
  kPasswordSharingRecipientsDownloader = 31,
  kWebHistoryService = 32,
  kComposeboxQueryController = 33,
  kDocumentSuggestionsService = 34,
  kEnterpriseSearchAggregator = 35,
  kParentPermissionDialog = 36,
  kUserCloudSigninRestrictionPolicyFetcher = 37,
  kCloudPolicyClientRegistration = 38,
  kSafeBrowsing = 39,
  kTailoredSecurityService = 40,
  kLensOverlayQueryController = 41,
  kTrustedVaultFrontend = 42,
  kFeedNetwork = 43,
  kAutofillPayments = 44,
  kPaymentsAccessTokenFetcher = 45,
  kSaveToDrive = 46,
  kFastPair = 47,
  kEduCoexistenceLoginHandler = 48,
  kEduAccountLoginHandler = 49,
  kChromeosFamilyLinkUserMetricsProvider = 50,
  kEnterpriseIdentityService = 51,
  kPromotionEligibilityChecker = 52,
  kPasswordManagerLeakDetection = 53,
  kAndroidManagementClient = 54,
  kArcBackgroundAuthCodeFetcher = 55,
  kGcmAccountTracker = 56,
  kPolicyTokenForwarder = 57,
  kPluginVmLicenseChecker = 58,
  kDrivefsAuth = 59,
  kNearbyPresenceServerClient = 60,
  kCryptAuthClient = 61,
  kAmbientMode = 62,
  kProfileDownloader = 63,
  kDataSharingAndroid = 64,
  kExtensionsIdentityAPI = 65,
  kManta = 66,
  kChromeMemex = 67,
  kDevtoolsAida = 68,
  kChromeOsBabelOrca = 69,
  kChromeOsBocaSchoolToolsAuth = 70,
  kSharedDataPreview = 71,
  kAccessCodeCastDiscovery = 72,
  kAuthServiceDriveApi = 73,
  kAuthServiceCalendar = 74,
  kAuthServiceGlanceablesClassroom = 75,
  kAuthServiceTasksClient = 76,
  kYouTubeMusic = 77,
  kContextualTasks = 78,
  kMaxValue = kContextualTasks,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:OAuthConsumerId)

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_OAUTH_CONSUMER_ID_H_
