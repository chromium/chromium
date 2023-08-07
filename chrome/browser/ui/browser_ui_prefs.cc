// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_ui_prefs.h"

#include <memory>
#include <string>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/accessibility/accessibility_features.h"
#endif

namespace {

uint32_t GetHomeButtonAndHomePageIsNewTabPageFlags() {
#if BUILDFLAG(IS_ANDROID)
  return PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  return user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
}

}  // namespace

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
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
  registry->RegisterIntegerPref(
      prefs::kMacRestoreLocationPermissionsExperimentCount, 0);
#endif  // BUILDFLAG(IS_MAC)

  registry->RegisterBooleanPref(prefs::kHoverCardImagesEnabled, true);
}

void RegisterBrowserUserPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true,
                                GetHomeButtonAndHomePageIsNewTabPageFlags());
  registry->RegisterBooleanPref(prefs::kShowHomeButton, false,
                                GetHomeButtonAndHomePageIsNewTabPageFlags());

  registry->RegisterInt64Pref(prefs::kDefaultBrowserLastDeclined, 0);
  bool reset_check_default = false;
#if BUILDFLAG(IS_WIN)
  reset_check_default = true;
#endif
  registry->RegisterBooleanPref(prefs::kResetCheckDefaultBrowser,
                                reset_check_default);
  registry->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop, true);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu, true);
  registry->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar, true);
  registry->RegisterBooleanPref(
      translate::prefs::kOfferTranslateEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(prefs::kCloudPrintEmail, std::string());
  registry->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled, true);
  registry->RegisterBooleanPref(prefs::kCloudPrintSubmitEnabled, true);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
  registry->RegisterDictionaryPref(prefs::kBrowserWindowPlacementPopup);
  registry->RegisterDictionaryPref(prefs::kAppWindowPlacement);
  registry->RegisterBooleanPref(
      prefs::kEnableDoNotTrack, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kPrintPreviewUseSystemDefaultPrinter,
                                false);
#endif
  registry->RegisterStringPref(prefs::kWebRTCIPHandlingPolicy,
                               blink::kWebRTCIPHandlingDefault);
  registry->RegisterStringPref(prefs::kWebRTCUDPPortRange, std::string());
  registry->RegisterBooleanPref(prefs::kWebRtcEventLogCollectionAllowed, false);
  registry->RegisterListPref(prefs::kWebRtcLocalIpsAllowedUrls);
  registry->RegisterBooleanPref(prefs::kWebRTCAllowLegacyTLSProtocols, false);
  registry->RegisterBooleanPref(prefs::kWebRtcTextLogCollectionAllowed, true);

  // Dictionaries to keep track of default tasks in the file browser.
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksByMimeType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDefaultTasksBySuffix,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

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
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::kAccessibilityFocusHighlightEnabled,
                                false);
#endif

  registry->RegisterBooleanPref(
      prefs::kHttpsOnlyModeEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kHttpAllowlist);
  registry->RegisterBooleanPref(prefs::kHttpsUpgradesEnabled, true);

  registry->RegisterDictionaryPref(prefs::kHttpsUpgradeFallbacks);
  registry->RegisterBooleanPref(prefs::kHttpsOnlyModeAutoEnabled, false);
}
