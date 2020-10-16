// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various preferences, for easier changing.

#ifndef CHROME_COMMON_PREF_NAMES_H_
#define CHROME_COMMON_PREF_NAMES_H_

#include <stddef.h>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"

namespace prefs {

// Profile prefs. Please add Local State prefs below instead.
extern const char kChildAccountStatusKnown[];
extern const char kDefaultApps[];
extern const char kSafeBrowsingForTrustedSourcesEnabled[];
extern const char kDisableScreenshots[];
extern const char kDownloadRestrictions[];
extern const char kForceEphemeralProfiles[];
extern const char kHomePageIsNewTabPage[];
extern const char kHomePage[];
extern const char kImportantSitesDialogHistory[];
extern const char kProfileCreationTime[];
#if defined(OS_WIN)
extern const char kLastProfileResetTimestamp[];
extern const char kChromeCleanerResetPending[];
#endif
extern const char kNewTabPageLocationOverride[];
extern const char kProfileIconVersion[];
extern const char kRestoreOnStartup[];
extern const char kSessionExitedCleanly[];
extern const char kSessionExitType[];
extern const char kSiteEngagementLastUpdateTime[];
#if BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
extern const char kSupervisedUserApprovedExtensions[];
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS) && BUILDFLAG(ENABLE_EXTENSIONS)
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
extern const char kSupervisedUserAllowlists[];
extern const char kURLsToRestoreOnStartup[];
extern const char kUserFeedbackAllowed[];

#if BUILDFLAG(ENABLE_RLZ)
extern const char kRlzPingDelaySeconds[];
#endif  // BUILDFLAG(ENABLE_RLZ)

// For OS_CHROMEOS we maintain the kApplicationLocale property in both local
// state and the user's profile.  The global property determines the locale of
// the login screen, while the user's profile determines their personal locale
// preference.
#if defined(OS_CHROMEOS)
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
extern const char kWebKitPictographFontFamily[];

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
extern const char kWebKitPictographFontFamilyMap[];

// Per-script font prefs that have defaults, for easy reference when registering
// the defaults.
extern const char kWebKitStandardFontFamilyArabic[];
#if defined(OS_WIN)
extern const char kWebKitFixedFontFamilyArabic[];
#endif
extern const char kWebKitSerifFontFamilyArabic[];
extern const char kWebKitSansSerifFontFamilyArabic[];
#if defined(OS_WIN)
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
#if defined(OS_WIN)
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
#if defined(OS_WIN) || defined(OS_MAC)
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
#if defined(OS_ANDROID)
extern const char kWebKitFontScaleFactor[];
extern const char kWebKitForceEnableZoom[];
extern const char kWebKitPasswordEchoEnabled[];
#endif
extern const char kSSLErrorOverrideAllowed[];
extern const char kIncognitoModeAvailability[];
extern const char kSearchSuggestEnabled[];
#if defined(OS_ANDROID)
extern const char kContextualSearchEnabled[];
extern const char kContextualSearchDisabledValue[];
extern const char kContextualSearchEnabledValue[];
#endif  // defined(OS_ANDROID)
extern const char kShowInternalAccessibilityTree[];
extern const char kAccessibilityImageLabelsEnabled[];
extern const char kAccessibilityImageLabelsOptInAccepted[];
#if defined(OS_ANDROID)
extern const char kAccessibilityImageLabelsEnabledAndroid[];
extern const char kAccessibilityImageLabelsOnlyOnWifi[];
#endif
#if !defined(OS_CHROMEOS)
extern const char kAccessibilityFocusHighlightEnabled[];
#endif
extern const char kAccessibilityCaptionsTextSize[];
extern const char kAccessibilityCaptionsTextFont[];
extern const char kAccessibilityCaptionsTextColor[];
extern const char kAccessibilityCaptionsTextOpacity[];
extern const char kAccessibilityCaptionsBackgroundColor[];
extern const char kAccessibilityCaptionsTextShadow[];
extern const char kAccessibilityCaptionsBackgroundOpacity[];
#if !defined(OS_ANDROID)
extern const char kLiveCaptionEnabled[];
extern const char kLiveCaptionLanguageCode[];
extern const char kSodaBinaryPath[];
extern const char kSodaEnUsConfigPath[];
extern const char kSodaJaJpConfigPath[];
#endif
#if defined(OS_MAC)
extern const char kConfirmToQuitEnabled[];
extern const char kShowFullscreenToolbar[];
extern const char kAllowJavascriptAppleEvents[];
#endif
extern const char kPromptForDownload[];
extern const char kQuicAllowed[];
extern const char kNetworkQualities[];
extern const char kNetworkEasterEggHighScore[];
#if defined(OS_ANDROID)
extern const char kLastPolicyCheckTime[];
#endif
extern const char kNetworkPredictionOptions[];
extern const char kDefaultAppsInstallState[];
extern const char kHideWebStoreIcon[];
#if defined(OS_CHROMEOS)
extern const char kAccountManagerNumTimesMigrationRanSuccessfully[];
extern const char kAccountManagerNumTimesWelcomeScreenShown[];
extern const char kTapToClickEnabled[];
extern const char kEnableTouchpadThreeFingerClick[];
extern const char kPrimaryMouseButtonRight[];
extern const char kMouseAcceleration[];
extern const char kMouseScrollAcceleration[];
extern const char kTouchpadAcceleration[];
extern const char kTouchpadScrollAcceleration[];
extern const char kMouseSensitivity[];
extern const char kMouseScrollSensitivity[];
extern const char kTouchpadSensitivity[];
extern const char kTouchpadScrollSensitivity[];
extern const char kUse24HourClock[];
extern const char kUserTimezone[];
extern const char kResolveTimezoneByGeolocation[];
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
extern const char kLanguageSendFunctionKeys[];

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
extern const char kAttestationExtensionAllowlist[];
extern const char kMultiProfileNeverShowIntro[];
extern const char kMultiProfileWarningShowDismissed[];
extern const char kMultiProfileUserBehavior[];
extern const char kFirstRunTutorialShown[];
extern const char kTimeOnOobe[];
extern const char kFileSystemProviderMounted[];
extern const char kTouchVirtualKeyboardEnabled[];
extern const char kCaptivePortalAuthenticationIgnoresProxy[];
extern const char kForceMaximizeOnFirstRun[];
extern const char kPlatformKeys[];
extern const char kUnifiedDesktopEnabledByDefault[];
extern const char kHatsLastInteractionTimestamp[];
extern const char kHatsSurveyCycleEndTimestamp[];
extern const char kHatsDeviceIsSelected[];
extern const char kQuickUnlockPinSecret[];
extern const char kQuickUnlockFingerprintRecord[];
extern const char kEolStatus[];
extern const char kEndOfLifeDate[];
extern const char kEolNotificationDismissed[];
extern const char kFirstEolWarningDismissed[];
extern const char kSecondEolWarningDismissed[];
extern const char kPinUnlockFeatureNotificationShown[];
extern const char kFingerprintUnlockFeatureNotificationShown[];
extern const char kQuickUnlockModeAllowlist[];
extern const char kQuickUnlockTimeout[];
extern const char kPinUnlockMinimumLength[];
extern const char kPinUnlockMaximumLength[];
extern const char kPinUnlockWeakPinsAllowed[];
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
extern const char kEnableSyncConsent[];
extern const char kNetworkFileSharesAllowed[];
extern const char kManagedSessionEnabled[];
extern const char kManagedSessionUseFullLoginWarning[];
extern const char kManagedGuestSessionAutoLaunchNotificationReduced[];
extern const char kTPMFirmwareUpdateCleanupDismissed[];
extern const char kTPMUpdatePlannedNotificationShownTime[];
extern const char kTPMUpdateOnNextRebootNotificationShown[];
extern const char kNetBiosShareDiscoveryEnabled[];
extern const char kChildScreenTimeMilliseconds[];
extern const char kLastChildScreenTimeSaved[];
extern const char kLastChildScreenTimeReset[];
extern const char kReleaseNotesLastShownMilestone[];
extern const char kReleaseNotesSuggestionChipTimesLeftToShow[];
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
extern const char kDeviceWallpaperImageFilePath[];
extern const char kKerberosRememberPasswordEnabled[];
extern const char kKerberosAddAccountsAllowed[];
extern const char kKerberosAccounts[];
extern const char kKerberosActivePrincipalName[];
extern const char kAppReinstallRecommendationEnabled[];
extern const char kStartupBrowserWindowLaunchSuppressed[];
extern const char kLoginExtensionApiDataForNextLoginAttempt[];
extern const char kLoginExtensionApiLaunchExtensionId[];
extern const char kSettingsShowOSBanner[];
extern const char kDeviceLoginScreenWebUsbAllowDevicesForUrls[];
extern const char kUpdateRequiredTimerStartTime[];
extern const char kUpdateRequiredWarningPeriod[];
extern const char kSystemProxyUserTrafficHostAndPort[];
extern const char kEduCoexistenceArcMigrationCompleted[];
#endif  // defined(OS_CHROMEOS)
extern const char kShowHomeButton[];
extern const char kSpeechRecognitionFilterProfanities[];
extern const char kAllowDeletingBrowserHistory[];
#if !defined(OS_ANDROID)
extern const char kHistoryMenuPromoShown[];
#endif
extern const char kForceGoogleSafeSearch[];
extern const char kForceYouTubeRestrict[];
extern const char kAllowedDomainsForApps[];
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kUsesSystemTheme[];
#endif
extern const char kCurrentThemePackFilename[];
extern const char kCurrentThemeID[];
extern const char kAutogeneratedThemeColor[];
extern const char kExtensionsUIDeveloperMode[];
extern const char kExtensionsUIDismissedADTPromo[];
extern const char kExtensionCommands[];
extern const char kPluginsLastInternalDirectory[];
extern const char kPluginsPluginsList[];
extern const char kPluginsDisabledPlugins[];
extern const char kPluginsDisabledPluginsExceptions[];
extern const char kPluginsEnabledPlugins[];
extern const char kPluginsAlwaysOpenPdfExternally[];
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kPluginsShowDetails[];
#endif
extern const char kPluginsAllowOutdated[];
extern const char kRunAllFlashInAllowMode[];
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kPluginsMetadata[];
extern const char kPluginsResourceCacheUpdate[];
#endif
extern const char kDefaultBrowserLastDeclined[];
extern const char kResetCheckDefaultBrowser[];
extern const char kDefaultBrowserSettingEnabled[];
#if defined(OS_MAC)
extern const char kShowUpdatePromotionInfoBar[];
#endif
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kUseCustomChromeFrame[];
#endif
#if BUILDFLAG(ENABLE_PLUGINS)
extern const char kContentSettingsPluginWhitelist[];
#endif
#if !defined(OS_ANDROID)
extern const char kPartitionDefaultZoomLevel[];
extern const char kPartitionPerHostZoomLevels[];

extern const char kPinnedTabs[];
#endif  // !defined(OS_ANDROID)

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

extern const char kProfileAvatarTutorialShown[];

extern const char kInvertNotificationShown[];

extern const char kPrinterTypeDenyList[];
extern const char kPrintingAllowedBackgroundGraphicsModes[];
extern const char kPrintingBackgroundGraphicsDefault[];
extern const char kPrintingPaperSizeDefault[];
extern const char kPrintingEnabled[];
extern const char kPrintHeaderFooter[];
extern const char kPrintPreviewDisabled[];
extern const char kPrintPreviewDefaultDestinationSelectionRules[];

#if defined(OS_WIN) && BUILDFLAG(ENABLE_PRINTING)
extern const char kPrintRasterizationMode[];
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
extern const char kPrintPreviewUseSystemDefaultPrinter[];
extern const char kUserDataSnapshotRetentionLimit[];
#endif

#if defined(OS_CHROMEOS)
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
extern const char kPrintingAPIExtensionsAllowlist[];
extern const char kDeletePrintJobHistoryAllowed[];
#endif  // OS_CHROMEOS

extern const char kDefaultSupervisedUserFilteringBehavior[];

extern const char kSupervisedUsers[];

extern const char kMessageCenterDisabledExtensionIds[];

extern const char kFullscreenAllowed[];

extern const char kLocalDiscoveryEnabled[];
extern const char kLocalDiscoveryNotificationsEnabled[];

#if defined(OS_ANDROID)
extern const char kMigratedToSiteNotificationChannels[];
extern const char kClearedBlockedSiteNotificationChannels[];
extern const char kUsageStatsEnabled[];
#endif

extern const char kPushMessagingAppIdentifierMap[];

extern const char kGCMProductCategoryForSubtypes[];

extern const char kEasyUnlockAllowed[];
extern const char kEasyUnlockPairing[];

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

#if !defined(OS_ANDROID)
extern const char kHasSeenWelcomePage[];
#endif

#if defined(OS_WIN)
// Only used in branded builds.
extern const char kNaviOnboardGroup[];
#endif  // defined(OS_WIN)

extern const char kQuietNotificationPermissionShouldShowPromo[];
extern const char kQuietNotificationPermissionPromoWasShown[];
extern const char kNotificationPermissionActions[];
extern const char kHadThreeConsecutiveNotificationPermissionDenies[];

extern const char kProfileLastUsed[];
extern const char kProfilesLastActive[];
extern const char kProfilesNumCreated[];
extern const char kProfileInfoCache[];
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
extern const char kLegacyProfileNamesMigrated[];
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
extern const char kProfileCreatedByVersion[];
extern const char kProfilesDeleted[];

extern const char kStabilityOtherUserCrashCount[];
extern const char kStabilityKernelCrashCount[];
extern const char kStabilitySystemUncleanShutdownCount[];

extern const char kStabilityPluginStats[];
extern const char kStabilityPluginName[];
extern const char kStabilityPluginLaunches[];
extern const char kStabilityPluginInstances[];
extern const char kStabilityPluginCrashes[];
extern const char kStabilityPluginLoadingErrors[];

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
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC)
extern const char kOpenPdfDownloadInSystemReader[];
#endif
#if defined(OS_ANDROID)
extern const char kPromptForDownloadAndroid[];
extern const char kDownloadLaterPromptStatus[];
extern const char kShowMissingSdCardErrorAndroid[];
#endif

extern const char kSaveFileDefaultDirectory[];
extern const char kSaveFileType[];

extern const char kAllowFileSelectionDialogs[];
extern const char kDefaultTasksByMimeType[];
extern const char kDefaultTasksBySuffix[];

extern const char kSharedClipboardEnabled[];

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
extern const char kClickToCallEnabled[];
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

extern const char kSelectFileLastDirectory[];

extern const char kProtocolHandlerPerOriginAllowedProtocols[];

extern const char kLastKnownIntranetRedirectOrigin[];
extern const char kDNSInterceptionChecksEnabled[];

extern const char kShutdownType[];
extern const char kShutdownNumProcesses[];
extern const char kShutdownNumProcessesSlow[];

extern const char kRestartLastSessionOnShutdown[];
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
extern const char kPromotionalTabsEnabled[];
extern const char kCommandLineFlagSecurityWarningsEnabled[];
#endif
extern const char kSuppressUnsupportedOSWarning[];
extern const char kWasRestarted[];
#endif  // !defined(OS_ANDROID)

extern const char kDisableExtensions[];

extern const char kNtpAppPageNames[];
extern const char kNtpCollapsedForeignSessions[];
#if defined(OS_ANDROID)
extern const char kNtpCollapsedRecentlyClosedTabs[];
extern const char kNtpCollapsedSnapshotDocument[];
extern const char kNtpCollapsedSyncPromo[];
#else
extern const char kNtpCustomBackgroundDict[];
extern const char kNtpCustomBackgroundLocalToDevice[];
extern const char kNtpPromoBlocklist[];
extern const char kNtpSearchSuggestionsBlocklist[];
extern const char kNtpSearchSuggestionsImpressions[];
extern const char kNtpSearchSuggestionsOptOut[];
extern const char kNtpShortcutsVisible[];
extern const char kNtpUseMostVisitedTiles[];
#endif  // defined(OS_ANDROID)
extern const char kNtpShownPage[];

extern const char kDevToolsAdbKey[];
extern const char kDevToolsAvailability[];
extern const char kDevToolsBackgroundServicesExpirationDict[];
extern const char kDevToolsDiscoverUsbDevicesEnabled[];
extern const char kDevToolsEditedFiles[];
extern const char kDevToolsFileSystemPaths[];
extern const char kDevToolsPortForwardingEnabled[];
extern const char kDevToolsPortForwardingDefaultSet[];
extern const char kDevToolsPortForwardingConfig[];
extern const char kDevToolsPreferences[];
extern const char kDevToolsDiscoverTCPTargetsEnabled[];
extern const char kDevToolsTCPDiscoveryConfig[];

#if !defined(OS_ANDROID)
extern const char kDiceSigninUserMenuPromoCount[];
#endif

extern const char kWebAppCreateOnDesktop[];
extern const char kWebAppCreateInAppsMenu[];
extern const char kWebAppCreateInQuickLaunchBar[];

extern const char kWebAppInstallForceList[];
extern const char kWebAppInstallMetrics[];
extern const char kWebAppsDailyMetrics[];
extern const char kWebAppsDailyMetricsDate[];
extern const char kWebAppsExtensionIDs[];
extern const char kWebAppsPreferences[];
extern const char kWebAppsUserDisplayModeCleanedUp[];
extern const char kSystemWebAppLastUpdateVersion[];
extern const char kSystemWebAppLastInstalledLocale[];
extern const char kSystemWebAppInstallFailureCount[];
extern const char kSystemWebAppLastAttemptedVersion[];
extern const char kSystemWebAppLastAttemptedLocale[];

extern const char kDefaultAudioCaptureDevice[];
extern const char kDefaultVideoCaptureDevice[];
extern const char kMediaDeviceIdSalt[];
extern const char kMediaStorageIdSalt[];

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
extern const char kCloudPrintDeprecationWarningsSuppressed[];

extern const char kMaxConnectionsPerProxy[];

extern const char kAudioCaptureAllowed[];
extern const char kAudioCaptureAllowedUrls[];
extern const char kVideoCaptureAllowed[];
extern const char kVideoCaptureAllowedUrls[];
extern const char kScreenCaptureAllowed[];

#if defined(OS_CHROMEOS)
extern const char kDemoModeConfig[];
extern const char kDemoModeCountry[];
extern const char kDemoModeDefaultLocale[];
extern const char kDeviceSettingsCache[];
extern const char kHardwareKeyboardLayout[];
extern const char kShouldAutoEnroll[];
extern const char kAutoEnrollmentPowerLimit[];
extern const char kShouldRetrieveDeviceState[];
extern const char kDeviceActivityTimes[];
extern const char kAppActivityTimes[];
extern const char kUserActivityTimes[];
extern const char kExternalStorageDisabled[];
extern const char kExternalStorageReadOnly[];
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
extern const char kTimesHIDDialogShown[];
extern const char kUsersLastInputMethod[];
extern const char kEchoCheckedOffers[];
extern const char kCachedMultiProfileUserBehavior[];
extern const char kInitialLocale[];
extern const char kOobeComplete[];
extern const char kOobeScreenPending[];
extern const char kOobeMarketingOptInScreenFinished[];
extern const char kDeviceRegistered[];
extern const char kEnrollmentRecoveryRequired[];
extern const char kHelpAppShouldShowGetStarted[];
extern const char kHelpAppShouldShowParentalControl[];
extern const char kHelpAppTabletModeDuringOobe[];
extern const char kUsedPolicyCertificates[];
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
extern const char kExtensionInstallEventLoggingEnabled[];
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

#endif  // defined(OS_CHROMEOS)

extern const char kClearPluginLSODataEnabled[];
extern const char kPepperFlashSettingsEnabled[];
extern const char kDiskCacheDir[];
extern const char kDiskCacheSize[];

extern const char kChromeOsReleaseChannel[];

extern const char kPerformanceTracingEnabled[];

extern const char kTabStripStackedLayout[];

extern const char kRegisteredBackgroundContents[];

extern const char kTotalMemoryLimitMb[];

extern const char kAuthSchemes[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerAllowlist[];
extern const char kAuthNegotiateDelegateAllowlist[];
extern const char kGSSAPILibraryName[];
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kAllowCrossOriginAuthPrompt[];
extern const char kGloballyScopeHTTPAuthCacheEnabled[];
extern const char kAmbientAuthenticationInPrivateModesEnabled[];

#if defined(OS_LINUX) || defined(OS_MAC) || defined(OS_CHROMEOS)
extern const char kAuthNegotiateDelegateByKdcPolicy[];
#endif  // defined(OS_LINUX) || defined(OS_MAC) || defined(OS_CHROMEOS)

#if defined(OS_POSIX)
extern const char kNtlmV2Enabled[];
#endif  // defined(OS_POSIX)

#if defined(OS_CHROMEOS)
extern const char kKerberosEnabled[];
#endif

extern const char kCertRevocationCheckingEnabled[];
extern const char kCertRevocationCheckingRequiredLocalAnchors[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionMax[];
extern const char kCipherSuiteBlacklist[];
extern const char kH2ClientCertCoalescingHosts[];
extern const char kHSTSPolicyBypassList[];

extern const char kBuiltInDnsClientEnabled[];
extern const char kDnsOverHttpsMode[];
extern const char kDnsOverHttpsTemplates[];

extern const char kRegisteredProtocolHandlers[];
extern const char kIgnoredProtocolHandlers[];
extern const char kPolicyRegisteredProtocolHandlers[];
extern const char kPolicyIgnoredProtocolHandlers[];
extern const char kCustomHandlersEnabled[];

#if defined(OS_MAC)
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

#if defined(OS_CHROMEOS)
extern const char kSigninScreenTimezone[];
extern const char kResolveDeviceTimezoneByGeolocation[];
extern const char kResolveDeviceTimezoneByGeolocationMethod[];
extern const char kSystemTimezoneAutomaticDetectionPolicy[];
#endif  // defined(OS_CHROMEOS)

extern const char kEnableMediaRouter[];
#if !defined(OS_ANDROID)
extern const char kShowCastIconInToolbar[];
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
extern const char kRelaunchNotification[];
extern const char kRelaunchNotificationPeriod[];
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
extern const char kRelaunchHeadsUpPeriod[];
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID)
extern const char kAttemptedToEnableAutoupdate[];

extern const char kMediaGalleriesUniqueId[];
extern const char kMediaGalleriesRememberedGalleries[];
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
extern const char kPolicyPinnedLauncherApps[];
extern const char kShelfDefaultPinLayoutRolls[];
extern const char kShelfDefaultPinLayoutRollsForTabletFormFactor[];
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
extern const char kNetworkProfileWarningsLeft[];
extern const char kNetworkProfileLastWarningTime[];
extern const char kShortcutMigrationVersion[];
#endif

#if defined(OS_CHROMEOS)
extern const char kRLZBrand[];
extern const char kRLZDisabled[];
extern const char kAppListLocalState[];
#endif

extern const char kAppShortcutsVersion[];
extern const char kAppShortcutsArch[];

extern const char kDRMSalt[];
extern const char kEnableDRM[];

extern const char kWatchdogExtensionActive[];

#if defined(OS_ANDROID)
extern const char kPartnerBookmarkMappings[];
#endif  // defined(OS_ANDROID)

extern const char kQuickCheckEnabled[];
extern const char kBrowserGuestModeEnabled[];
extern const char kBrowserGuestModeEnforced[];
extern const char kBrowserAddPersonEnabled[];
extern const char kForceBrowserSignin[];
extern const char kBrowserShowProfilePickerOnStartup[];
extern const char kSigninAllowedOnNextStartup[];

extern const char kCryptAuthDeviceId[];
extern const char kCryptAuthInstanceId[];
extern const char kCryptAuthInstanceIdToken[];
extern const char kEasyUnlockHardlockState[];
extern const char kEasyUnlockLocalStateTpmKeys[];
extern const char kEasyUnlockLocalStateUserPrefs[];

extern const char kRecoveryComponentNeedsElevation[];

extern const char kRegisteredSupervisedUserWhitelists[];

#if !defined(OS_ANDROID)
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

extern const char kBackgroundTracingLastUpload[];

extern const char kAllowDinosaurEasterEgg[];

#if defined(OS_ANDROID)
extern const char kClickedUpdateMenuItem[];
extern const char kLatestVersionWhenClickedUpdateMenuItem[];
#endif

extern const char kOriginTrialPublicKey[];
extern const char kOriginTrialDisabledFeatures[];
extern const char kOriginTrialDisabledTokens[];

extern const char kComponentUpdatesEnabled[];

#if defined(OS_ANDROID)
extern const char kSearchGeolocationDisclosureDismissed[];
extern const char kSearchGeolocationDisclosureShownCount[];
extern const char kSearchGeolocationDisclosureLastShowDate[];
extern const char kSearchGeolocationPreDisclosureMetricsRecorded[];
extern const char kSearchGeolocationPostDisclosureMetricsRecorded[];
#endif

extern const char kDSEGeolocationSettingDeprecated[];

extern const char kDSEPermissionsSettings[];
extern const char kDSEWasDisabledByPolicy[];

extern const char kWebShareVisitedTargets[];

#if defined(OS_WIN)
// Only used in branded builds.
extern const char kIncompatibleApplications[];
extern const char kModuleBlacklistCacheMD5Digest[];
extern const char kThirdPartyBlockingEnabled[];
#endif  // defined(OS_WIN)

// Windows mitigation policies.
#if defined(OS_WIN)
extern const char kRendererCodeIntegrityEnabled[];
#endif  // defined(OS_WIN)

extern const char kSettingsResetPromptPromptWave[];
extern const char kSettingsResetPromptLastTriggeredForDefaultSearch[];
extern const char kSettingsResetPromptLastTriggeredForStartupUrls[];
extern const char kSettingsResetPromptLastTriggeredForHomepage[];

#if defined(OS_ANDROID)
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

extern const char kUnsafelyTreatInsecureOriginAsSecure[];

extern const char kIsolateOrigins[];
extern const char kSitePerProcess[];

#if !defined(OS_ANDROID)
extern const char kAutoplayAllowed[];
extern const char kAutoplayWhitelist[];
extern const char kBlockAutoplayEnabled[];
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extern const char kAllowNativeNotifications[];
#endif

extern const char kNotificationNextPersistentId[];
extern const char kNotificationNextTriggerTime[];

extern const char kTabFreezingEnabled[];

extern const char kEnterpriseHardwarePlatformAPIEnabled[];

extern const char kSignedHTTPExchangeEnabled[];

extern const char kAllowPopupsDuringPageUnload[];

extern const char kAllowSyncXHRInPageDismissal[];

#if defined(OS_ANDROID)
extern const char kUsageStatsEnabled[];
#endif

#if defined(OS_CHROMEOS)
extern const char kClientCertificateManagementAllowed[];
extern const char kCACertificateManagementAllowed[];
#endif

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_POLICY_SUPPORTED)
extern const char kBuiltinCertificateVerifierEnabled[];
#endif

extern const char kSharingVapidKey[];
extern const char kSharingFCMRegistration[];
extern const char kSharingLocalSharingInfo[];

#if !defined(OS_ANDROID)
extern const char kHatsSurveyMetadata[];
#endif  // !defined(OS_ANDROID)

extern const char kExternalProtocolDialogShowAlwaysOpenCheckbox[];

extern const char kAutoLaunchProtocolsFromOrigins[];

extern const char kScrollToTextFragmentEnabled[];

#if defined(OS_ANDROID)
extern const char kKnownInterceptionDisclosureInfobarLastShown[];
#endif

#if defined(OS_CHROMEOS)
extern const char kRequiredClientCertificateForUser[];
extern const char kRequiredClientCertificateForDevice[];
extern const char kCertificateProvisioningStateForUser[];
extern const char kCertificateProvisioningStateForDevice[];
#endif

extern const char kMediaFeedsBackgroundFetching[];
extern const char kMediaFeedsSafeSearchEnabled[];

extern const char kAppCacheForceEnabled[];

#if defined(OS_CHROMEOS)
extern const char kAdbSideloadingDisallowedNotificationShown[];
extern const char kAdbSideloadingPowerwashPlannedNotificationShownTime[];
extern const char kAdbSideloadingPowerwashOnNextRebootNotificationShown[];
#endif

#if !defined(OS_ANDROID)
extern const char kCaretBrowsingEnabled[];
extern const char kShowCaretBrowsingDialog[];
#endif

}  // namespace prefs

#endif  // CHROME_COMMON_PREF_NAMES_H_
