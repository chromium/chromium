// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include <stddef.h>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"

namespace prefs {

// Profile prefs. Please add Local State prefs below instead.
extern const char kChildAccountStatusKnown[];
extern const char kPreinstalledApps[];
extern const char kSafeBrowsingForTrustedSourcesEnabled[];
extern const char kDisableScreenshots[];
extern const char kDownloadRestrictions[];
extern const char kDownloadBubbleEnabled[];
extern const char kForceEphemeralProfiles[];
extern const char kHomePageIsNewTabPage[];
extern const char kHomePage[];
extern const char kHttpsOnlyModeEnabled[];
extern const char kImportantSitesDialogHistory[];
extern const char kProfileCreationTime[];
#if BUILDFLAG(IS_WIN)
extern const char kLastProfileResetTimestamp[];
extern const char kChromeCleanerResetPending[];
extern const char kChromeCleanerScanCompletionTime[];
#endif
extern const char kNewTabPageLocationOverride[];
extern const char kProfileIconVersion[];
extern const char kRestoreOnStartup[];
extern const char kSessionExitType[];
#if !BUILDFLAG(IS_ANDROID)
extern const char kManagedProfileSerialAllowAllPortsForUrlsDeprecated[];
extern const char kManagedProfileSerialAllowUsbDevicesForUrlsDeprecated[];
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kSupervisedUserApprovedExtensions[];
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kSupervisedUserMetricsDayId[];
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
extern const char kSupervisedUserCustodianEmail[];
extern const char kSupervisedUserCustodianName[];
extern const char kSupervisedUserCustodianObfuscatedGaiaId[];
extern const char kSupervisedUserCustodianProfileImageURL[];
extern const char kSupervisedUserCustodianProfileURL[];
extern const char kSupervisedUserExtensionsMayRequestPermissions[];
extern const char kSupervisedUserManualHosts[];
extern const char kSupervisedUserManualURLs[];
extern const char kSupervisedUserSafeSites[];
extern const char kSupervisedUserSecondCustodianEmail[];
extern const char kSupervisedUserSecondCustodianName[];
extern const char kSupervisedUserSecondCustodianObfuscatedGaiaId[];
extern const char kSupervisedUserSecondCustodianProfileImageURL[];
extern const char kSupervisedUserSecondCustodianProfileURL[];
extern const char kSupervisedUserSharedSettings[];
extern const char kURLsToRestoreOnStartup[];
extern const char kUserFeedbackAllowed[];

#if BUILDFLAG(ENABLE_RLZ)
extern const char kRlzPingDelaySeconds[];
#endif  // BUILDFLAG(ENABLE_RLZ)

// For OS_CHROMEOS we maintain the kApplicationLocale property in both local
// state and the user's profile. The global property determines the locale of
// the login screen, while the user's profile determines their personal locale
// preference.
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kApplicationLocaleBackup[];
extern const char kApplicationLocaleAccepted[];
extern const char kOwnerLocale[];
extern const char kAllowedLanguages[];
#endif

extern const char kDefaultCharset[];
extern const char kWebKitCommonScript[];
extern const char kWebKitStandardFontFamily[];
extern const char kWebKitFixedFontFamily[];
extern const char kWebKitSerifFontFamily[];
extern const char kWebKitSansSerifFontFamily[];
extern const char kWebKitCursiveFontFamily[];
extern const char kWebKitFantasyFontFamily[];
extern const char kWebKitMathFontFamily[];

// ISO 15924 four-letter script codes that per-script font prefs are supported
// for.
extern const char* const kWebKitScriptsForFontFamilyMaps[];
extern const size_t kWebKitScriptsForFontFamilyMapsLength;

// Per-script font pref prefixes.
extern const char kWebKitStandardFontFamilyMap[];
extern const char kWebKitFixedFontFamilyMap[];
extern const char kWebKitSerifFontFamilyMap[];
extern const char kWebKitSansSerifFontFamilyMap[];
extern const char kWebKitCursiveFontFamilyMap[];
extern const char kWebKitFantasyFontFamilyMap[];
extern const char kWebKitMathFontFamilyMap[];

// Per-script font prefs that have defaults, for easy reference when registering
// the defaults.
extern const char kWebKitStandardFontFamilyArabic[];
#if BUILDFLAG(IS_WIN)
extern const char kWebKitFixedFontFamilyArabic[];
#endif
extern const char kWebKitSerifFontFamilyArabic[];
extern const char kWebKitSansSerifFontFamilyArabic[];
#if BUILDFLAG(IS_WIN)
extern const char kWebKitStandardFontFamilyCyrillic[];
extern const char kWebKitFixedFontFamilyCyrillic[];
extern const char kWebKitSerifFontFamilyCyrillic[];
extern const char kWebKitSansSerifFontFamilyCyrillic[];
extern const char kWebKitStandardFontFamilyGreek[];
extern const char kWebKitFixedFontFamilyGreek[];
extern const char kWebKitSerifFontFamilyGreek[];
extern const char kWebKitSansSerifFontFamilyGreek[];
#endif
extern const char kWebKitStandardFontFamilyJapanese[];
extern const char kWebKitFixedFontFamilyJapanese[];
extern const char kWebKitSerifFontFamilyJapanese[];
extern const char kWebKitSansSerifFontFamilyJapanese[];
extern const char kWebKitStandardFontFamilyKorean[];
extern const char kWebKitFixedFontFamilyKorean[];
extern const char kWebKitSerifFontFamilyKorean[];
extern const char kWebKitSansSerifFontFamilyKorean[];
#if BUILDFLAG(IS_WIN)
extern const char kWebKitCursiveFontFamilyKorean[];
#endif
extern const char kWebKitStandardFontFamilySimplifiedHan[];
extern const char kWebKitFixedFontFamilySimplifiedHan[];
extern const char kWebKitSerifFontFamilySimplifiedHan[];
extern const char kWebKitSansSerifFontFamilySimplifiedHan[];
extern const char kWebKitStandardFontFamilyTraditionalHan[];
extern const char kWebKitFixedFontFamilyTraditionalHan[];
extern const char kWebKitSerifFontFamilyTraditionalHan[];
extern const char kWebKitSansSerifFontFamilyTraditionalHan[];
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
extern const char kWebKitCursiveFontFamilySimplifiedHan[];
extern const char kWebKitCursiveFontFamilyTraditionalHan[];
#endif

extern const char kWebKitDefaultFontSize[];
extern const char kWebKitDefaultFixedFontSize[];
extern const char kWebKitMinimumFontSize[];
extern const char kWebKitMinimumLogicalFontSize[];
extern const char kWebKitJavascriptEnabled[];
extern const char kWebKitWebSecurityEnabled[];
extern const char kWebKitLoadsImagesAutomatically[];
extern const char kWebKitPluginsEnabled[];
extern const char kWebKitDomPasteEnabled[];
extern const char kWebKitTextAreasAreResizable[];
extern const char kWebKitJavascriptCanAccessClipboard[];
extern const char kWebkitTabsToLinks[];
extern const char kWebKitAllowRunningInsecureContent[];
extern const char kWebKitForceDarkModeEnabled[];
#if BUILDFLAG(IS_ANDROID)
extern const char kWebKitPasswordEchoEnabled[];
#endif
extern const char kSSLErrorOverrideAllowed[];
extern const char kSSLErrorOverrideAllowedForOrigins[];
extern const char kIncognitoModeAvailability[];
extern const char kSearchSuggestEnabled[];
#if BUILDFLAG(IS_ANDROID)
extern const char kContextualSearchEnabled[];
extern const char kContextualSearchDisabledValue[];
extern const char kContextualSearchEnabledValue[];
extern const char kContextualSearchPromoCardShownCount[];
extern const char kContextualSearchWasFullyPrivacyEnabled[];
#endif  // BUILDFLAG(IS_ANDROID)
extern const char kShowInternalAccessibilityTree[];
extern const char kAccessibilityImageLabelsEnabled[];
extern const char kAccessibilityImageLabelsOptInAccepted[];
#if BUILDFLAG(IS_ANDROID)
extern const char kAccessibilityImageLabelsEnabledAndroid[];
extern const char kAccessibilityImageLabelsOnlyOnWifi[];
#endif
#if !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kAccessibilityFocusHighlightEnabled[];
#endif
#if !BUILDFLAG(IS_ANDROID)
extern const char kLiveCaptionEnabled[];
extern const char kLiveCaptionLanguageCode[];
#endif
extern const char kPageColors[];
extern const char kApplyPageColorsOnlyOnIncreasedContrast[];
#if BUILDFLAG(IS_WIN)
extern const char kIsDefaultPageColorsOnHighContrast[];
#endif
#if BUILDFLAG(IS_MAC)
extern const char kConfirmToQuitEnabled[];
extern const char kShowFullscreenToolbar[];
extern const char kAllowJavascriptAppleEvents[];
#endif
extern const char kPromptForDownload[];
extern const char kQuicAllowed[];
extern const char kNetworkQualities[];
extern const char kNetworkEasterEggHighScore[];
extern const char kNetworkPredictionOptions[];
extern const char kPreinstalledAppsInstallState[];
extern const char kHideWebStoreIcon[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kAttestationExtensionAllowlist[];
extern const char kPrintingAPIExtensionsAllowlist[];
extern const char kEnableSyncConsent[];
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kTapToClickEnabled[];
extern const char kEnableTouchpadThreeFingerClick[];
extern const char kPrimaryMouseButtonRight[];
extern const char kPrimaryPointingStickButtonRight[];
extern const char kOwnerPrimaryPointingStickButtonRight[];
extern const char kMouseAcceleration[];
extern const char kMouseScrollAcceleration[];
extern const char kPointingStickAcceleration[];
extern const char kTouchpadAcceleration[];
extern const char kTouchpadScrollAcceleration[];
extern const char kTouchpadHapticFeedback[];
extern const char kTouchpadHapticClickSensitivity[];
extern const char kMouseSensitivity[];
extern const char kMouseScrollSensitivity[];
extern const char kTouchpadSensitivity[];
extern const char kTouchpadScrollSensitivity[];
extern const char kPointingStickSensitivity[];
extern const char kUse24HourClock[];
extern const char kUserTimezone[];
extern const char kResolveTimezoneByGeolocationMethod[];
extern const char kResolveTimezoneByGeolocationMigratedToMethod[];
// TODO(yusukes): Change "kLanguageABC" to "kABC". The current form is too long
// to remember and confusing. The prefs are actually for input methods and i18n
// keyboards, not UI languages.
extern const char kLanguageCurrentInputMethod[];
extern const char kLanguagePreviousInputMethod[];
extern const char kLanguageAllowedInputMethods[];
extern const char kLanguagePreloadEngines[];
extern const char kLanguagePreloadEnginesSyncable[];
extern const char kLanguageEnabledImes[];
extern const char kLanguageEnabledImesSyncable[];
extern const char kLanguageImeMenuActivated[];
extern const char kLanguageInputMethodSpecificSettings[];
extern const char kLanguageShouldMergeInputMethods[];

extern const char kLabsAdvancedFilesystemEnabled[];
extern const char kLabsMediaplayerEnabled[];
extern const char kShowMobileDataNotification[];
extern const char kChromeOSReleaseNotesVersion[];
extern const char kNoteTakingAppId[];
extern const char kNoteTakingAppEnabledOnLockScreen[];
extern const char kNoteTakingAppsLockScreenAllowlist[];
extern const char kNoteTakingAppsLockScreenToastShown[];
extern const char kRestoreLastLockScreenNote[];
extern const char kSessionUserActivitySeen[];
extern const char kSessionStartTime[];
extern const char kSessionLengthLimit[];
extern const char kSessionWaitForInitialUserActivity[];
extern const char kLastSessionType[];
extern const char kLastSessionLength[];
extern const char kTermsOfServiceURL[];
extern const char kAttestationEnabled[];
extern const char kMultiProfileNeverShowIntro[];
extern const char kMultiProfileWarningShowDismissed[];
extern const char kMultiProfileUserBehavior[];
extern const char kFirstRunTutorialShown[];
extern const char kTimeOnOobe[];
extern const char kFileSystemProviderMounted[];
extern const char kTouchVirtualKeyboardEnabled[];
extern const char kCaptivePortalAuthenticationIgnoresProxy[];
extern const char kPlatformKeys[];
extern const char kKeyPermissionsOneTimeMigrationDone[];
extern const char kUnifiedDesktopEnabledByDefault[];
extern const char kHatsLastInteractionTimestamp[];
extern const char kHatsSurveyCycleEndTimestamp[];
extern const char kHatsDeviceIsSelected[];
extern const char kHatsEntSurveyCycleEndTs[];
extern const char kHatsEntDeviceIsSelected[];
extern const char kHatsStabilitySurveyCycleEndTs[];
extern const char kHatsStabilityDeviceIsSelected[];
extern const char kHatsPerformanceSurveyCycleEndTs[];
extern const char kHatsPerformanceDeviceIsSelected[];
extern const char kHatsOnboardingSurveyCycleEndTs[];
extern const char kHatsOnboardingDeviceIsSelected[];
extern const char kHatsUnlockDeviceIsSelected[];
extern const char kHatsUnlockSurveyCycleEndTs[];
extern const char kHatsSmartLockDeviceIsSelected[];
extern const char kHatsSmartLockSurveyCycleEndTs[];
extern const char kHatsArcGamesDeviceIsSelected[];
extern const char kHatsArcGamesSurveyCycleEndTs[];
extern const char kHatsAudioDeviceIsSelected[];
extern const char kHatsAudioSurveyCycleEndTs[];
extern const char kHatsPersonalizationAvatarSurveyCycleEndTs[];
extern const char kHatsPersonalizationAvatarSurveyIsSelected[];
extern const char kHatsPersonalizationScreensaverSurveyCycleEndTs[];
extern const char kHatsPersonalizationScreensaverSurveyIsSelected[];
extern const char kHatsPersonalizationWallpaperSurveyCycleEndTs[];
extern const char kHatsPersonalizationWallpaperSurveyIsSelected[];
extern const char kHatsMediaAppPdfCycleEndTs[];
extern const char kHatsMediaAppPdfIsSelected[];
extern const char kHatsCameraAppDeviceIsSelected[];
extern const char kHatsCameraAppSurveyCycleEndTs[];
extern const char kHatsPhotosExperienceCycleEndTs[];
extern const char kHatsPhotosExperienceIsSelected[];
extern const char kHatsGeneralCameraIsSelected[];
extern const char kHatsGeneralCameraSurveyCycleEndTs[];
extern const char kEolStatus[];
extern const char kEndOfLifeDate[];
extern const char kEolNotificationDismissed[];
extern const char kFirstEolWarningDismissed[];
extern const char kSecondEolWarningDismissed[];
extern const char kPinUnlockFeatureNotificationShown[];
extern const char kFingerprintUnlockFeatureNotificationShown[];
extern const char kPinUnlockAutosubmitEnabled[];
extern const char kCastReceiverEnabled[];
extern const char kMinimumAllowedChromeVersion[];
extern const char kShowArcSettingsOnSessionStart[];
extern const char kShowSyncSettingsOnSessionStart[];
extern const char kTextToSpeechLangToVoiceName[];
extern const char kTextToSpeechRate[];
extern const char kTextToSpeechPitch[];
extern const char kTextToSpeechVolume[];
extern const char kTimeLimitLocalOverride[];
extern const char kUsageTimeLimit[];
extern const char kScreenTimeLastState[];
extern const char kNetworkFileSharesAllowed[];
extern const char kManagedSessionUseFullLoginWarning[];
extern const char kTPMFirmwareUpdateCleanupDismissed[];
extern const char kTPMUpdatePlannedNotificationShownTime[];
extern const char kTPMUpdateOnNextRebootNotificationShown[];
extern const char kNetBiosShareDiscoveryEnabled[];
extern const char kChildScreenTimeMilliseconds[];
extern const char kLastChildScreenTimeSaved[];
extern const char kLastChildScreenTimeReset[];
extern const char kHelpAppNotificationLastShownMilestone[];
extern const char kReleaseNotesSuggestionChipTimesLeftToShow[];
extern const char kDiscoverTabSuggestionChipTimesLeftToShow[];
extern const char kNTLMShareAuthenticationEnabled[];
extern const char kNetworkFileSharesPreconfiguredShares[];
extern const char kMostRecentlyUsedNetworkFileShareURL[];
extern const char kNetworkFileSharesSavedShares[];
extern const char kParentAccessCodeConfig[];
extern const char kPerAppTimeLimitsAppActivities[];
extern const char kPerAppTimeLimitsLastResetTime[];
extern const char kPerAppTimeLimitsLastSuccessfulReportTime[];
extern const char kPerAppTimeLimitsLatestLimitUpdateTime[];
extern const char kPerAppTimeLimitsPolicy[];
extern const char kPerAppTimeLimitsAllowlistPolicy[];
extern const char kFamilyUserMetricsDayId[];
extern const char kFamilyUserMetricsSessionEngagementDuration[];
extern const char kFamilyUserMetricsChromeBrowserEngagementDuration[];
extern const char kDeviceWallpaperImageFilePath[];
extern const char kKerberosRememberPasswordEnabled[];
extern const char kKerberosAddAccountsAllowed[];
extern const char kKerberosAccounts[];
extern const char kKerberosActivePrincipalName[];
extern const char kKerberosDomainAutocomplete[];
extern const char kKerberosDefaultConfiguration[];
extern const char kAppReinstallRecommendationEnabled[];
extern const char kStartupBrowserWindowLaunchSuppressed[];
extern const char kLoginExtensionApiDataForNextLoginAttempt[];
extern const char kLoginExtensionApiCanLockManagedGuestSession[];
extern const char kUpdateRequiredTimerStartTime[];
extern const char kUpdateRequiredWarningPeriod[];
extern const char kSystemProxyUserTrafficHostAndPort[];
extern const char kEduCoexistenceArcMigrationCompleted[];
extern const char kSharedStorage[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS)
extern const char kDeskAPIThirdPartyAccessEnabled[];
extern const char kDeskAPIThirdPartyAllowlist[];
extern const char kForceMaximizeOnFirstRun[];
extern const char kInsightsExtensionEnabled[];
extern const char kOOMKillsDailyCount[];
extern const char kOOMKillsDailySample[];
extern const char kRestrictedManagedGuestSessionExtensionCleanupExemptList[];
extern const char kUsedPolicyCertificates[];
#endif  // BUILDFLAG(IS_CHROMEOS)
extern const char kShowHomeButton[];
extern const char kSpeechRecognitionFilterProfanities[];
extern const char kAllowDeletingBrowserHistory[];
extern const char kForceGoogleSafeSearch[];
extern const char kForceYouTubeRestrict[];
extern const char kAllowedDomainsForApps[];
#if BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kUseAshProxy[];
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(https://crbug.com/1317782): Remove in M110.
extern const char kUsesSystemThemeDeprecated[];
extern const char kSystemTheme[];
#endif
extern const char kCurrentThemePackFilename[];
extern const char kCurrentThemeID[];
extern const char kAutogeneratedThemeColor[];
extern const char kPolicyThemeColor[];
extern const char kExtensionsUIDeveloperMode[];
extern const char kExtensionsUIDismissedADTPromo[];
extern const char kExtensionCommands[];
extern const char kPluginsLastInternalDirectory[];
extern const char kPluginsPluginsList[];
extern const char kPluginsAlwaysOpenPdfExternally[];
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kPluginsShowDetails[];
#endif
extern const char kPluginsAllowOutdated[];
extern const char kDefaultBrowserLastDeclined[];
extern const char kResetCheckDefaultBrowser[];
extern const char kDefaultBrowserSettingEnabled[];
#if BUILDFLAG(IS_MAC)
extern const char kShowUpdatePromotionInfoBar[];
#endif
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kUseCustomChromeFrame[];
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kContentSettingsPluginAllowlist[];
#endif
extern const char kPartitionDefaultZoomLevel[];
extern const char kPartitionPerHostZoomLevels[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kPinnedTabs[];
#endif  // !BUILDFLAG(IS_ANDROID)

extern const char kDisable3DAPIs[];
extern const char kEnableHyperlinkAuditing[];
extern const char kEnableReferrers[];
extern const char kEnableDoNotTrack[];
extern const char kEnableEncryptedMedia[];

extern const char kImportAutofillFormData[];
extern const char kImportBookmarks[];
extern const char kImportHistory[];
extern const char kImportHomepage[];
extern const char kImportSavedPasswords[];
extern const char kImportSearchEngine[];

extern const char kImportDialogAutofillFormData[];
extern const char kImportDialogBookmarks[];
extern const char kImportDialogHistory[];
extern const char kImportDialogSavedPasswords[];
extern const char kImportDialogSearchEngine[];

extern const char kProfileAvatarIndex[];
extern const char kProfileUsingDefaultName[];
extern const char kProfileName[];
extern const char kProfileUsingDefaultAvatar[];
extern const char kProfileUsingGAIAAvatar[];
extern const char kSupervisedUserId[];

extern const char kInvertNotificationShown[];

extern const char kPrinterTypeDenyList[];
extern const char kPrintingAllowedBackgroundGraphicsModes[];
extern const char kPrintingBackgroundGraphicsDefault[];
extern const char kPrintingPaperSizeDefault[];

#if BUILDFLAG(ENABLE_PRINTING)
extern const char kPrintingEnabled[];
#endif  // BUILDFLAG(ENABLE_PRINTING)

extern const char kPrintHeaderFooter[];
extern const char kPrintPreviewDisabled[];
extern const char kPrintPreviewDefaultDestinationSelectionRules[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
extern const char kPrintPdfAsImageAvailability[];
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
extern const char kPrintRasterizePdfDpi[];
extern const char kPrintPdfAsImageDefault[];
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_PRINTING)
extern const char kPrintPostScriptMode[];
extern const char kPrintRasterizationMode[];
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
extern const char kPrintPreviewUseSystemDefaultPrinter[];
extern const char kUserDataSnapshotRetentionLimit[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kExternalPrintServersAllowlist[];
extern const char kDeviceExternalPrintServersAllowlist[];
extern const char kRecommendedPrinters[];
extern const char kRecommendedPrintersAccessMode[];
extern const char kRecommendedPrintersBlocklist[];
extern const char kRecommendedPrintersAllowlist[];
extern const char kUserPrintersAllowed[];

extern const char kPrintingAllowedColorModes[];
extern const char kPrintingAllowedDuplexModes[];
extern const char kPrintingAllowedPinModes[];
extern const char kPrintingColorDefault[];
extern const char kPrintingDuplexDefault[];
extern const char kPrintingPinDefault[];
extern const char kPrintingSendUsernameAndFilenameEnabled[];
extern const char kPrintingMaxSheetsAllowed[];
extern const char kPrintJobHistoryExpirationPeriod[];
extern const char kDeletePrintJobHistoryAllowed[];
extern const char kPrintingClientNameTemplate[];
extern const char kPrintingOAuth2AuthorizationServers[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kDefaultSupervisedUserFilteringBehavior[];

extern const char kSupervisedUsers[];

extern const char kMessageCenterDisabledExtensionIds[];

extern const char kFullscreenAllowed[];

#if BUILDFLAG(IS_ANDROID)
extern const char kMigratedToSiteNotificationChannels[];
extern const char kClearedBlockedSiteNotificationChannels[];
extern const char kUsageStatsEnabled[];
#endif

extern const char kPushMessagingAppIdentifierMap[];

extern const char kGCMProductCategoryForSubtypes[];

extern const char kEasyUnlockAllowed[];
extern const char kEasyUnlockPairing[];
extern const char kHasSeenSmartLockSignInRemovedNotification[];

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kToolbarIconSurfacingBubbleAcknowledged[];
extern const char kToolbarIconSurfacingBubbleLastShowTime[];
#endif

extern const char kWebRTCMultipleRoutesEnabled[];
extern const char kWebRTCNonProxiedUdpEnabled[];
extern const char kWebRTCIPHandlingPolicy[];
extern const char kWebRTCUDPPortRange[];
extern const char kWebRtcEventLogCollectionAllowed[];
extern const char kWebRtcLocalIpsAllowedUrls[];
extern const char kWebRTCAllowLegacyTLSProtocols[];

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
extern const char kFirstRunFinished[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kHasSeenWelcomePage[];
extern const char kManagedAccountsSigninRestriction[];
extern const char kManagedAccountsSigninRestrictionScopeMachine[];
#if !BUILDFLAG(IS_CHROMEOS)
extern const char kEnterpriseProfileCreationKeepBrowsingData[];
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
// Only used in branded builds.
extern const char kNaviOnboardGroup[];
#endif  // BUILDFLAG(IS_WIN)

extern const char kQuietNotificationPermissionShouldShowPromo[];
extern const char kQuietNotificationPermissionPromoWasShown[];
extern const char kNotificationPermissionActions[];
extern const char kHadThreeConsecutiveNotificationPermissionDenies[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kManagedSerialAllowAllPortsForUrls[];
extern const char kManagedSerialAllowUsbDevicesForUrls[];
extern const char kManagedWebHidAllowAllDevicesForUrls[];
extern const char kManagedWebHidAllowDevicesForUrls[];
extern const char kManagedWebHidAllowDevicesWithHidUsagesForUrls[];
#endif  // !BUILDFLAG(IS_ANDROID)

extern const char kProfileLastUsed[];
extern const char kProfilesLastActive[];
extern const char kProfilesNumCreated[];
extern const char kProfileAttributes[];
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kLegacyProfileNamesMigrated[];
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kProfileCreatedByVersion[];
extern const char kProfilesDeleted[];

extern const char kStabilityOtherUserCrashCount[];
extern const char kStabilityKernelCrashCount[];
extern const char kStabilitySystemUncleanShutdownCount[];

extern const char kBrowserSuppressDefaultBrowserPrompt[];

extern const char kBrowserWindowPlacement[];
extern const char kBrowserWindowPlacementPopup[];
extern const char kTaskManagerWindowPlacement[];
extern const char kTaskManagerColumnVisibility[];
extern const char kTaskManagerEndProcessEnabled[];
extern const char kAppWindowPlacement[];

extern const char kDownloadDefaultDirectory[];
extern const char kDownloadExtensionsToOpen[];
extern const char kDownloadExtensionsToOpenByPolicy[];
extern const char kDownloadAllowedURLsForOpenByPolicy[];
extern const char kDownloadDirUpgraded[];
extern const char kDownloadLastCompleteTime[];
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
extern const char kOpenPdfDownloadInSystemReader[];
#endif
#if BUILDFLAG(IS_ANDROID)
extern const char kPromptForDownloadAndroid[];
extern const char kShowMissingSdCardErrorAndroid[];
extern const char kIncognitoReauthenticationForAndroid[];
#endif

extern const char kSaveFileDefaultDirectory[];
extern const char kSaveFileType[];

extern const char kAllowFileSelectionDialogs[];

extern const char kDefaultTasksByMimeType[];
extern const char kDefaultTasksBySuffix[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kDefaultHandlersForFileExtensions[];
extern const char kOfficeSetupComplete[];
extern const char kOfficeFilesAlwaysMove[];
#endif

extern const char kSharedClipboardEnabled[];

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
extern const char kClickToCallEnabled[];
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

extern const char kSelectFileLastDirectory[];

extern const char kProtocolHandlerPerOriginAllowedProtocols[];

extern const char kLastKnownIntranetRedirectOrigin[];
extern const char kDNSInterceptionChecksEnabled[];
extern const char kIntranetRedirectBehavior[];

extern const char kShutdownType[];
extern const char kShutdownNumProcesses[];
extern const char kShutdownNumProcessesSlow[];

extern const char kRestartLastSessionOnShutdown[];
#if !BUILDFLAG(IS_ANDROID)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kCommandLineFlagSecurityWarningsEnabled[];
#endif
extern const char kPromotionalTabsEnabled[];
extern const char kSuppressUnsupportedOSWarning[];
extern const char kWasRestarted[];
#endif  // !BUILDFLAG(IS_ANDROID)

extern const char kDisableExtensions[];

extern const char kNtpAppPageNames[];
extern const char kNtpCollapsedForeignSessions[];
#if BUILDFLAG(IS_ANDROID)
extern const char kNtpCollapsedRecentlyClosedTabs[];
extern const char kNtpCollapsedSnapshotDocument[];
extern const char kNtpCollapsedSyncPromo[];
#else
extern const char kNtpCustomBackgroundDict[];
extern const char kNtpCustomBackgroundLocalToDevice[];
extern const char kNtpDisabledModules[];
extern const char kNtpModulesOrder[];
extern const char kNtpModulesVisible[];
extern const char kNtpModulesShownCount[];
extern const char kNtpModulesFirstShownTime[];
extern const char kNtpModulesFreVisible[];
extern const char kNtpPromoBlocklist[];
extern const char kNtpPromoVisible[];
#endif  // BUILDFLAG(IS_ANDROID)
extern const char kNtpShownPage[];

extern const char kDevToolsAdbKey[];
extern const char kDevToolsAvailability[];
extern const char kDevToolsRemoteDebuggingAllowed[];
extern const char kDevToolsBackgroundServicesExpirationDict[];
extern const char kDevToolsDiscoverUsbDevicesEnabled[];
extern const char kDevToolsEditedFiles[];
extern const char kDevToolsFileSystemPaths[];
extern const char kDevToolsPortForwardingEnabled[];
extern const char kDevToolsPortForwardingDefaultSet[];
extern const char kDevToolsPortForwardingConfig[];
extern const char kDevToolsPreferences[];
extern const char kDevToolsSyncPreferences[];
extern const char kDevToolsSyncedPreferencesSyncEnabled[];
extern const char kDevToolsSyncedPreferencesSyncDisabled[];
extern const char kDevToolsDiscoverTCPTargetsEnabled[];
extern const char kDevToolsTCPDiscoveryConfig[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kDiceSigninUserMenuPromoCount[];
#endif

extern const char kUserUninstalledPreinstalledWebAppPref[];
extern const char kManagedConfigurationPerOrigin[];
extern const char kLastManagedConfigurationHashForOrigin[];

extern const char kWebAppCreateOnDesktop[];
extern const char kWebAppCreateInAppsMenu[];
extern const char kWebAppCreateInQuickLaunchBar[];
extern const char kWebAppInstallForceList[];
extern const char kWebAppSettings[];
extern const char kWebAppInstallMetrics[];
extern const char kWebAppsDailyMetrics[];
extern const char kWebAppsDailyMetricsDate[];
extern const char kWebAppsExtensionIDs[];
extern const char kWebAppsAppAgnosticIphState[];
extern const char kWebAppsLastPreinstallSynchronizeVersion[];
extern const char kWebAppsDidMigrateDefaultChromeApps[];
extern const char kWebAppsUninstalledDefaultChromeApps[];
extern const char kWebAppsPreferences[];
extern const char kWebAppsIsolationState[];

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
extern const char kWebAppsUrlHandlerInfo[];
#endif

extern const char kDefaultAudioCaptureDevice[];
extern const char kDefaultVideoCaptureDevice[];
extern const char kMediaDeviceIdSalt[];
extern const char kMediaStorageIdSalt[];
#if BUILDFLAG(IS_WIN)
extern const char kMediaCdmOriginData[];
extern const char kNetworkServiceSandboxEnabled[];
#endif  // BUILDFLAG(IS_WIN)

extern const char kPrintPreviewStickySettings[];
extern const char kCloudPrintRoot[];
extern const char kCloudPrintProxyEnabled[];
extern const char kCloudPrintProxyId[];
extern const char kCloudPrintAuthToken[];
extern const char kCloudPrintEmail[];
extern const char kCloudPrintPrintSystemSettings[];
extern const char kCloudPrintEnableJobPoll[];
extern const char kCloudPrintRobotRefreshToken[];
extern const char kCloudPrintRobotEmail[];
extern const char kCloudPrintConnectNewPrinters[];
extern const char kCloudPrintXmppPingEnabled[];
extern const char kCloudPrintXmppPingTimeout[];
extern const char kCloudPrintPrinters[];
extern const char kCloudPrintSubmitEnabled[];
extern const char kCloudPrintUserSettings[];

extern const char kMaxConnectionsPerProxy[];

extern const char kAudioCaptureAllowed[];
extern const char kAudioCaptureAllowedUrls[];
extern const char kVideoCaptureAllowed[];
extern const char kVideoCaptureAllowedUrls[];
extern const char kScreenCaptureAllowed[];
extern const char kScreenCaptureAllowedByOrigins[];
extern const char kWindowCaptureAllowedByOrigins[];
extern const char kTabCaptureAllowedByOrigins[];
extern const char kSameOriginTabCaptureAllowedByOrigins[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kDemoModeConfig[];
extern const char kDemoModeCountry[];
extern const char kDemoModeRetailerId[];
extern const char kDemoModeStoreId[];
extern const char kDemoModeDefaultLocale[];
extern const char kDeviceSettingsCache[];
extern const char kHardwareKeyboardLayout[];
extern const char kShouldAutoEnroll[];
extern const char kAutoEnrollmentPowerLimit[];
extern const char kShouldRetrieveDeviceState[];
extern const char kEnrollmentPsmResult[];
extern const char kEnrollmentPsmDeterminationTime[];
extern const char kDeviceActivityTimes[];
extern const char kAppActivityTimes[];
extern const char kUserActivityTimes[];
extern const char kOwnerPrimaryMouseButtonRight[];
extern const char kOwnerTapToClickEnabled[];
extern const char kUptimeLimit[];
extern const char kRebootAfterUpdate[];
extern const char kDeviceRobotAnyApiRefreshToken[];
extern const char kDeviceEnrollmentRequisition[];
extern const char kDeviceEnrollmentSubOrganization[];
extern const char kDeviceEnrollmentAutoStart[];
extern const char kDeviceEnrollmentCanExit[];
extern const char kDeviceDMToken[];
extern const char kCachedMultiProfileUserBehavior[];
extern const char kInitialLocale[];
extern const char kDeviceRegistered[];
extern const char kEnrollmentRecoveryRequired[];
extern const char kHelpAppShouldShowGetStarted[];
extern const char kHelpAppShouldShowParentalControl[];
extern const char kHelpAppTabletModeDuringOobe[];
extern const char kServerBackedDeviceState[];
extern const char kCustomizationDefaultWallpaperURL[];
extern const char kLogoutStartedLast[];
extern const char kConsumerManagementStage[];
extern const char kReportArcStatusEnabled[];
extern const char kSchedulerConfiguration[];
extern const char kNetworkThrottlingEnabled[];
extern const char kPowerMetricsDailySample[];
extern const char kPowerMetricsIdleScreenDimCount[];
extern const char kPowerMetricsIdleScreenOffCount[];
extern const char kPowerMetricsIdleSuspendCount[];
extern const char kPowerMetricsLidClosedSuspendCount[];
extern const char kReportingUsers[];
extern const char kArcAppInstallEventLoggingEnabled[];
extern const char kRemoveUsersRemoteCommand[];
extern const char kAutoScreenBrightnessMetricsDailySample[];
extern const char kAutoScreenBrightnessMetricsAtlasUserAdjustmentCount[];
extern const char kAutoScreenBrightnessMetricsEveUserAdjustmentCount[];
extern const char kAutoScreenBrightnessMetricsNocturneUserAdjustmentCount[];
extern const char kAutoScreenBrightnessMetricsKohakuUserAdjustmentCount[];
extern const char kAutoScreenBrightnessMetricsNoAlsUserAdjustmentCount[];
extern const char kAutoScreenBrightnessMetricsSupportedAlsUserAdjustmentCount[];
extern const char
    kAutoScreenBrightnessMetricsUnsupportedAlsUserAdjustmentCount[];
extern const char kKnownUserParentAccessCodeConfig[];
extern const char kLastRsuDeviceIdUploaded[];
extern const char kDeviceName[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kClearPluginLSODataEnabled[];
extern const char kPepperFlashSettingsEnabled[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];

extern const char kChromeOsReleaseChannel[];

extern const char kPerformanceTracingEnabled[];

extern const char kRegisteredBackgroundContents[];

extern const char kTotalMemoryLimitMb[];

extern const char kAuthSchemes[];
extern const char kAllHttpAuthSchemesAllowedForOrigins[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerAllowlist[];
extern const char kAuthNegotiateDelegateAllowlist[];
extern const char kGSSAPILibraryName[];
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kGloballyScopeHTTPAuthCacheEnabled[];
extern const char kAmbientAuthenticationInPrivateModesEnabled[];
extern const char kBasicAuthOverHttpEnabled[];

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
extern const char kAuthNegotiateDelegateByKdcPolicy[];
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
extern const char kNtlmV2Enabled[];
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kKerberosEnabled[];
extern const char kIsolatedWebAppInstallForceList[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
extern const char kCloudApAuthEnabled[];
#endif  // BUILDFLAG(IS_WIN)

extern const char kCertRevocationCheckingEnabled[];
extern const char kCertRevocationCheckingRequiredLocalAnchors[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionMax[];
extern const char kCipherSuiteBlacklist[];
extern const char kH2ClientCertCoalescingHosts[];
extern const char kHSTSPolicyBypassList[];
extern const char kCECPQ2Enabled[];
extern const char kEncryptedClientHelloEnabled[];

extern const char kBuiltInDnsClientEnabled[];
extern const char kDnsOverHttpsMode[];
extern const char kDnsOverHttpsTemplates[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kDnsOverHttpsTemplatesWithIdentifiers[];
extern const char kDnsOverHttpsSalt[];
#endif  // BUILDFLAG(IS_CHROMEOS)
extern const char kAdditionalDnsQueryTypesEnabled[];

#if BUILDFLAG(IS_MAC)
extern const char kUserRemovedLoginItem[];
extern const char kChromeCreatedLoginItem[];
extern const char kMigratedLoginItemPref[];
extern const char kNotifyWhenAppsKeepChromeAlive[];
#endif

extern const char kBackgroundModeEnabled[];
extern const char kHardwareAccelerationModeEnabled[];
extern const char kHardwareAccelerationModePrevious[];

extern const char kDevicePolicyRefreshRate[];

extern const char kFactoryResetRequested[];
extern const char kForceFactoryReset[];
extern const char kFactoryResetTPMFirmwareUpdateMode[];
extern const char kDebuggingFeaturesRequested[];
extern const char kEnableAdbSideloadingRequested[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kSigninScreenTimezone[];
extern const char kResolveDeviceTimezoneByGeolocationMethod[];
extern const char kSystemTimezoneAutomaticDetectionPolicy[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

extern const char kEnableMediaRouter[];
#if !BUILDFLAG(IS_ANDROID)
extern const char kShowCastIconInToolbar[];
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
extern const char kRelaunchNotification[];
extern const char kRelaunchNotificationPeriod[];
extern const char kRelaunchWindow[];
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kRelaunchHeadsUpPeriod[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_MAC)
extern const char kMacRestoreLocationPermissionsExperimentCount[];
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kEnrollmentIdUploadedOnChromad[];
extern const char kLastChromadMigrationAttemptTime[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
extern const char kHardwareSecureDecryptionDisabledTimes[];
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
extern const char kKioskMetrics[];
extern const char kNewWindowsInKioskAllowed[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
extern const char kAttemptedToEnableAutoupdate[];

extern const char kMediaGalleriesUniqueId[];
extern const char kMediaGalleriesRememberedGalleries[];
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kPolicyPinnedLauncherApps[];
extern const char kShelfDefaultPinLayoutRolls[];
extern const char kShelfDefaultPinLayoutRollsForTabletFormFactor[];
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
extern const char kNetworkProfileWarningsLeft[];
extern const char kNetworkProfileLastWarningTime[];
extern const char kShortcutMigrationVersion[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kRLZBrand[];
extern const char kRLZDisabled[];
extern const char kAppListLocalState[];
extern const char kAppListPreferredOrder[];
#endif

extern const char kAppShortcutsVersion[];
extern const char kAppShortcutsArch[];

extern const char kProtectedContentDefault[];

extern const char kWatchdogExtensionActive[];

#if BUILDFLAG(IS_ANDROID)
extern const char kPartnerBookmarkMappings[];
#endif  // BUILDFLAG(IS_ANDROID)

extern const char kQuickCheckEnabled[];
extern const char kBrowserGuestModeEnabled[];
extern const char kBrowserGuestModeEnforced[];
extern const char kBrowserAddPersonEnabled[];
extern const char kForceBrowserSignin[];
extern const char kBrowserProfilePickerAvailabilityOnStartup[];
extern const char kBrowserProfilePickerShown[];
extern const char kBrowserShowProfilePickerOnStartup[];
extern const char kSigninAllowedOnNextStartup[];
extern const char kSigninInterceptionEnabled[];
#if BUILDFLAG(IS_CHROMEOS)
extern const char kEchoCheckedOffers[];
extern const char kLacrosSecondaryProfilesAllowed[];
extern const char kLacrosDataBackwardMigrationMode[];
#endif  // BUILDFLAG(IS_CHROMEOS)

extern const char kCryptAuthDeviceId[];
extern const char kCryptAuthInstanceId[];
extern const char kCryptAuthInstanceIdToken[];
extern const char kEasyUnlockHardlockState[];
extern const char kEasyUnlockLocalStateTpmKeys[];
extern const char kEasyUnlockLocalStateUserPrefs[];

extern const char kRecoveryComponentNeedsElevation[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kCloudExtensionRequestEnabled[];
extern const char kCloudExtensionRequestIds[];
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
extern const char kRestartInBackground[];
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kAnimationPolicy[];
extern const char kSecurityKeyPermitAttestation[];
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// FocusHighlight is special as the feature also exists in lacros.
// However, extensions can only set the ash-value (computed here
// and later sent to ash).
extern const char kLacrosAccessibilityFocusHighlightEnabled[];

extern const char kLacrosAccessibilityAutoclickEnabled[];
extern const char kLacrosAccessibilityCaretHighlightEnabled[];
extern const char kLacrosAccessibilityCursorColorEnabled[];
extern const char kLacrosAccessibilityCursorHighlightEnabled[];
extern const char kLacrosAccessibilityDictationEnabled[];
extern const char kLacrosAccessibilityHighContrastEnabled[];
extern const char kLacrosAccessibilityLargeCursorEnabled[];
extern const char kLacrosAccessibilityScreenMagnifierEnabled[];
extern const char kLacrosAccessibilitySelectToSpeakEnabled[];
extern const char kLacrosAccessibilitySpokenFeedbackEnabled[];
extern const char kLacrosAccessibilityStickyKeysEnabled[];
extern const char kLacrosAccessibilitySwitchAccessEnabled[];
extern const char kLacrosAccessibilityVirtualKeyboardEnabled[];
extern const char kLacrosDockedMagnifierEnabled[];
#endif

extern const char kAllowDinosaurEasterEgg[];

#if BUILDFLAG(IS_ANDROID)
extern const char kClickedUpdateMenuItem[];
extern const char kLatestVersionWhenClickedUpdateMenuItem[];
#endif

#if BUILDFLAG(IS_ANDROID)
extern const char kCommerceMerchantViewerMessagesShownTime[];
#endif

extern const char kDSEGeolocationSettingDeprecated[];

extern const char kDSEPermissionsSettings[];
extern const char kDSEWasDisabledByPolicy[];

extern const char kWebShareVisitedTargets[];

#if BUILDFLAG(IS_WIN)
// Only used in branded builds.
extern const char kIncompatibleApplications[];
extern const char kModuleBlocklistCacheMD5Digest[];
extern const char kThirdPartyBlockingEnabled[];
#endif  // BUILDFLAG(IS_WIN)

// Windows mitigation policies.
#if BUILDFLAG(IS_WIN)
extern const char kRendererCodeIntegrityEnabled[];
extern const char kRendererAppContainerEnabled[];
extern const char kBlockBrowserLegacyExtensionPoints[];
#endif  // BUILDFLAG(IS_WIN)

extern const char kSettingsResetPromptPromptWave[];
extern const char kSettingsResetPromptLastTriggeredForDefaultSearch[];
extern const char kSettingsResetPromptLastTriggeredForStartupUrls[];
extern const char kSettingsResetPromptLastTriggeredForHomepage[];

#if BUILDFLAG(IS_ANDROID)
extern const char kClipboardLastModifiedTime[];
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
extern const char kOfflineUsageStartObserved[];
extern const char kOfflineUsageOnlineObserved[];
extern const char kOfflineUsageOfflineObserved[];
extern const char kPrefetchUsageEnabledObserved[];
extern const char kPrefetchUsageFetchObserved[];
extern const char kPrefetchUsageOpenObserved[];
extern const char kOfflineUsageTrackingDay[];
extern const char kOfflineUsageUnusedCount[];
extern const char kOfflineUsageStartedCount[];
extern const char kOfflineUsageOfflineCount[];
extern const char kOfflineUsageOnlineCount[];
extern const char kOfflineUsageMixedCount[];
extern const char kPrefetchUsageEnabledCount[];
extern const char kPrefetchUsageFetchedCount[];
extern const char kPrefetchUsageOpenedCount[];
extern const char kPrefetchUsageMixedCount[];
#endif

extern const char kMediaEngagementSchemaVersion[];

// Preferences for recording metrics about tab and window usage.
extern const char kTabStatsTotalTabCountMax[];
extern const char kTabStatsMaxTabsPerWindow[];
extern const char kTabStatsWindowCountMax[];
extern const char kTabStatsDailySample[];
extern const char kTabStatsDiscardsExternal[];
extern const char kTabStatsDiscardsUrgent[];
extern const char kTabStatsReloadsExternal[];
extern const char kTabStatsReloadsUrgent[];

extern const char kUnsafelyTreatInsecureOriginAsSecure[];

extern const char kIsolateOrigins[];
extern const char kSitePerProcess[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kSharedArrayBufferUnrestrictedAccessAllowed[];
extern const char kAutoplayAllowed[];
extern const char kAutoplayAllowlist[];
extern const char kBlockAutoplayEnabled[];
#endif
extern const char kSandboxExternalProtocolBlocked[];

#if BUILDFLAG(IS_LINUX)
extern const char kAllowSystemNotifications[];
#endif

extern const char kNotificationNextPersistentId[];
extern const char kNotificationNextTriggerTime[];

extern const char kTabFreezingEnabled[];

extern const char kEnterpriseHardwarePlatformAPIEnabled[];

extern const char kSignedHTTPExchangeEnabled[];

#if BUILDFLAG(IS_ANDROID)
extern const char kUsageStatsEnabled[];
#endif

#if BUILDFLAG(IS_CHROMEOS)
extern const char kClientCertificateManagementAllowed[];
extern const char kCACertificateManagementAllowed[];
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_POLICY_SUPPORTED)
extern const char kChromeRootStoreEnabled[];
#endif

extern const char kSharingVapidKey[];
extern const char kSharingFCMRegistration[];
extern const char kSharingLocalSharingInfo[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kHatsSurveyMetadata[];
#endif  // !BUILDFLAG(IS_ANDROID)

extern const char kExternalProtocolDialogShowAlwaysOpenCheckbox[];

extern const char kAutoLaunchProtocolsFromOrigins[];

extern const char kScrollToTextFragmentEnabled[];

#if BUILDFLAG(IS_ANDROID)
extern const char kKnownInterceptionDisclosureInfobarLastShown[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kRequiredClientCertificateForUser[];
extern const char kRequiredClientCertificateForDevice[];
extern const char kCertificateProvisioningStateForUser[];
extern const char kCertificateProvisioningStateForDevice[];
#endif
extern const char kPromptOnMultipleMatchingCertificates[];

extern const char kMediaFeedsBackgroundFetching[];
extern const char kMediaFeedsSafeSearchEnabled[];
extern const char kMediaFeedsAutoSelectEnabled[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kAdbSideloadingDisallowedNotificationShown[];
extern const char kAdbSideloadingPowerwashPlannedNotificationShownTime[];
extern const char kAdbSideloadingPowerwashOnNextRebootNotificationShown[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kCaretBrowsingEnabled[];
extern const char kShowCaretBrowsingDialog[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kLacrosLaunchSwitch[];
extern const char kLacrosSelection[];
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kSecurityTokenSessionBehavior[];
extern const char kSecurityTokenSessionNotificationSeconds[];
extern const char kSecurityTokenSessionNotificationScheduledDomain[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kCartModuleHidden[];
extern const char kCartModuleWelcomeSurfaceShownTimes[];
extern const char kCartDiscountAcknowledged[];
extern const char kCartDiscountEnabled[];
extern const char kCartUsedDiscounts[];
extern const char kCartDiscountLastFetchedTime[];
extern const char kCartDiscountConsentShown[];
extern const char kDiscountConsentDecisionMadeIn[];
extern const char kDiscountConsentDismissedIn[];
extern const char kDiscountConsentLastDimissedTime[];
extern const char kDiscountConsentLastShownInVariation[];
extern const char kDiscountConsentPastDismissedCount[];
extern const char kDiscountConsentShowInterest[];
extern const char kDiscountConsentShowInterestIn[];
#endif

#if BUILDFLAG(IS_ANDROID)
extern const char kWebXRImmersiveArEnabled[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kFetchKeepaliveDurationOnShutdown[];
#endif

extern const char kSuppressDifferentOriginSubframeJSDialogs[];

extern const char kUserAgentReduction[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kPdfAnnotationsEnabled[];
#endif

extern const char kExplicitlyAllowedNetworkPorts[];

#if !BUILDFLAG(IS_ANDROID)
extern const char kDeviceAttributesAllowedForOrigins[];
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
extern const char kDesktopSharingHubEnabled[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kLastWhatsNewVersion[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kLensRegionSearchEnabled[];
extern const char kSidePanelHorizontalAlignment[];
extern const char kLensDesktopNTPSearchEnabled[];
#endif

extern const char kPrivacyGuideViewed[];

extern const char kCorsNonWildcardRequestHeadersSupport[];

extern const char kOriginAgentClusterDefaultEnabled[];

extern const char kForceMajorVersionToMinorPositionInUserAgent[];

extern const char kIdleTimeout[];
extern const char kIdleTimeoutActions[];

extern const char kSCTAuditingHashdanceReportCount[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
extern const char kConsumerAutoUpdateToggle[];
extern const char kHindiInscriptLayoutEnabled[];
#endif

#if !BUILDFLAG(IS_ANDROID)
extern const char kHighEfficiencyChipExpandedCount[];

extern const char kShouldShowPriceTrackFUEBubble[];
extern const char kShouldShowSidePanelBookmarkTab[];
#endif

extern const char kStrictMimetypeCheckForWorkerScriptsEnabled[];

#if BUILDFLAG(IS_ANDROID)
extern const char kVirtualKeyboardResizesLayoutByDefault[];
#endif

extern const char kAccessControlAllowMethodsInCORSPreflightSpecConformant[];

extern const char kDIPSTimerLastUpdate[];

extern const char kThrottleNonVisibleCrossOriginIframesAllowed[];
extern const char kNewBaseUrlInheritanceBehaviorAllowed[];

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
