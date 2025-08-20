// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OAUTH_CONSUMER_IDS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OAUTH_CONSUMER_IDS_H_

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
  kMaxValue = kFeedbackUploader,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:OAuthConsumerId)

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_OAUTH_CONSUMER_IDS_H_
