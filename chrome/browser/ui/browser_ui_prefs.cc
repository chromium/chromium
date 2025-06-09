// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include <memory>
#include <string>

#include "base/numerics/safe_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sharing_message/buildflags.h"
#include "components/sharing_message/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if !BUILDFLAG(IS_CHROMEOS)
#include "ui/accessibility/accessibility_features.h"
#endif

void RegisterBrowserPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kRelaunchNotification, 0);
  registry->RegisterIntegerPref(
      prefs::kRelaunchNotificationPeriod,
      base::saturated_cast<int>(
          UpgradeDetector::GetDefaultHighAnnoyanceThreshold()
              .InMilliseconds()));
  registry->RegisterDictionaryPref(prefs::kRelaunchWindow);
  registry->RegisterIntegerPref(prefs::kRelaunchFastIfOutdated, 0);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  registry->RegisterIntegerPref(
      installer_downloader::prefs::kInstallerDownloaderInfobarShowCount, 0);
  registry->RegisterBooleanPref(
      installer_downloader::prefs::kInstallerDownloaderPreventFutureDisplay,
      false);
  registry->RegisterBooleanPref(
      installer_downloader::prefs::kInstallerDownloaderBypassEligibilityCheck,
      false);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC)
  registry->RegisterIntegerPref(
      prefs::kMacRestoreLocationPermissionsExperimentCount, 0);
#endif  // BUILDFLAG(IS_MAC)

  registry->RegisterBooleanPref(prefs::kHoverCardImagesEnabled, true);

  registry->RegisterBooleanPref(prefs::kHoverCardMemoryUsageEnabled, true);

#if defined(USE_AURA)
  registry->RegisterBooleanPref(prefs::kOverscrollHistoryNavigationEnabled,
                                true);
#endif
  registry->RegisterTimePref(prefs::kDefaultBrowserLastDeclinedTime,
                             base::Time());
  registry->RegisterIntegerPref(prefs::kDefaultBrowserDeclinedCount, 0);
  registry->RegisterTimePref(prefs::kDefaultBrowserFirstShownTime,
                             base::Time());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  registry->RegisterTimePref(prefs::kPdfInfoBarLastShown, base::Time());
  registry->RegisterIntegerPref(prefs::kPdfInfoBarTimesShown, 0);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  registry->RegisterStringPref(prefs::kEnterpriseCustomLabelForBrowser,
                               std::string());
  registry->RegisterStringPref(prefs::kEnterpriseLogoUrlForBrowser,
                               std::string());
  registry->RegisterBooleanPref(prefs::kNTPFooterManagementNoticeEnabled, true);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
#if BUILDFLAG(IS_ANDROID)
  const uint32_t pref_registration_flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  const uint32_t pref_registration_flags =
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif

  registry->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true,
                                pref_registration_flags);
  registry->RegisterBooleanPref(prefs::kShowHomeButton, false,
                                pref_registration_flags);

  registry->RegisterBooleanPref(prefs::kShowForwardButton, true,
                                pref_registration_flags);

  registry->RegisterBooleanPref(prefs::kPinSplitTabButton, false,
                                pref_registration_flags);

  registry->RegisterInt64Pref(prefs::kDefaultBrowserLastDeclined, 0);
  registry->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop, true);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu, true);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar, true);
  registry->RegisterBooleanPref(
      translate::prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kCloudPrintEmail, std::string());
  registry->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled, true);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacementPopup);
  registry->RegisterDictionaryPref(prefs::kAppWindowPlacement);
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kPrintPreviewUseSystemDefaultPrinter,
                                false);
#endif
  registry->RegisterStringPref(prefs::kWebRTCIPHandlingPolicy,
                               blink::kWebRTCIPHandlingDefault);
  registry->RegisterListPref(prefs::kWebRTCIPHandlingUrl, base::Value::List());
  registry->RegisterStringPref(prefs::kWebRTCUDPPortRange, std::string());
  registry->RegisterBooleanPref(prefs::kWebRtcEventLogCollectionAllowed, false);
  registry->RegisterListPref(prefs::kWebRtcLocalIpsAllowedUrls);
  registry->RegisterBooleanPref(prefs::kWebRtcTextLogCollectionAllowed, true);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  registry->RegisterBooleanPref(policy::policy_prefs::kHideWebStoreIcon, false);
  registry->RegisterBooleanPref(prefs::kSharedClipboardEnabled, true);

#if BUILDFLAG(ENABLE_CLICK_TO_CALL)
  registry->RegisterBooleanPref(prefs::kClickToCallEnabled, true);
#endif  // BUILDFLAG(ENABLE_CLICK_TO_CALL)

#if BUILDFLAG(IS_MAC)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time,
  // StartupBrowserCreator has already created the browser window, and it's too
  // late: we need the pref to be already initialized. Doing it here also saves
  // us from having to hard-code pref registration in the several unit tests
  // that use this preference.
  registry->RegisterBooleanPref(prefs::kShowUpdatePromotionInfoBar, true);
  registry->RegisterBooleanPref(
      prefs::kShowFullscreenToolbar, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAllowJavascriptAppleEvents, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#else
  registry->RegisterBooleanPref(prefs::kFullscreenAllowed, true);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kForceMaximizeOnFirstRun, false);
#endif

  registry->RegisterBooleanPref(prefs::kEnterpriseHardwarePlatformAPIEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kUserFeedbackAllowed, true);
  registry->RegisterBooleanPref(
      prefs::kExternalProtocolDialogShowAlwaysOpenCheckbox, true);
  registry->RegisterBooleanPref(prefs::kScreenCaptureAllowed, true);
  registry->RegisterListPref(prefs::kScreenCaptureAllowedByOrigins);
  registry->RegisterListPref(prefs::kWindowCaptureAllowedByOrigins);
  registry->RegisterListPref(prefs::kTabCaptureAllowedByOrigins);
  registry->RegisterListPref(prefs::kSameOriginTabCaptureAllowedByOrigins);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kCaretBrowsingEnabled, false);
  registry->RegisterBooleanPref(prefs::kShowCaretBrowsingDialog, true);
  registry->RegisterBooleanPref(prefs::kNTPFooterExtensionAttributionEnabled,
                                true);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kAccessibilityFocusHighlightEnabled,
                                false);
#endif

  registry->RegisterBooleanPref(
      prefs::kHttpsOnlyModeEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kHttpsFirstBalancedMode, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kHttpsFirstModeIncognito, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kHttpAllowlist);
  registry->RegisterBooleanPref(prefs::kHttpsUpgradesEnabled, true);

  registry->RegisterDictionaryPref(prefs::kHttpsUpgradeFallbacks);
  registry->RegisterDictionaryPref(prefs::kHttpsUpgradeNavigations);
  registry->RegisterBooleanPref(prefs::kHttpsOnlyModeAutoEnabled, false);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  registry->RegisterStringPref(prefs::kEnterpriseLogoUrlForProfile,
                               std::string());
  registry->RegisterStringPref(prefs::kEnterpriseCustomLabelForProfile,
                               std::string());
  registry->RegisterIntegerPref(prefs::kEnterpriseProfileBadgeToolbarSettings,
                                0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}
