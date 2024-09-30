// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/infobar_utils.h"

#include "base/command_line.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/obsolete_system/obsolete_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/session_crashed_bubble.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/bidding_and_auction_consented_debugging_infobar_delegate.h"
#include "chrome/browser/ui/startup/google_api_keys_infobar_delegate.h"
#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/startup/test_third_party_cookie_phaseout_infobar_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/network_switches.h"

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#endif

#if BUILDFLAG(CHROME_FOR_TESTING)
#include "chrome/browser/ui/startup/chrome_for_testing_infobar_delegate.h"
#endif

namespace {
bool ShouldShowBadFlagsSecurityWarnings() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return true;

  const auto* pref = local_state->FindPreference(
      prefs::kCommandLineFlagSecurityWarningsEnabled);
  DCHECK(pref);

  // The warnings can only be disabled by policy. Default to show warnings.
  if (pref->IsManaged())
    return pref->GetValue()->GetBool();
#endif
  return true;
}

// This is a separate function to avoid accidentally reading the switch from
// `startup_command_line`.
bool IsAutomationEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAutomation);
}

// This is a separate function to avoid accidentally reading the switch from
// `startup_command_line`.
bool IsKioskModeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kKioskMode);
}

#if BUILDFLAG(CHROME_FOR_TESTING)
bool IsGpuTest() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kTestType) == "gpu";
}
#endif

}  // namespace

void AddInfoBarsIfNecessary(Browser* browser,
                            Profile* profile,
                            const base::CommandLine& startup_command_line,
                            chrome::startup::IsFirstRun is_first_run,
                            bool is_web_app) {
  if (!browser || !profile || browser->tab_strip_model()->count() == 0) {
    return;
  }

  // Show the Automation info bar unless it has been disabled by policy.
  bool show_bad_flags_security_warnings = ShouldShowBadFlagsSecurityWarnings();

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);

  if (show_bad_flags_security_warnings) {
#if BUILDFLAG(CHROME_FOR_TESTING)
    if (!IsGpuTest()) {
      ChromeForTestingInfoBarDelegate::Create();
    }
#endif

    if (IsAutomationEnabled()) {
      AutomationInfoBarDelegate::Create();
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kProtectedAudiencesConsentedDebugToken)) {
      BiddingAndAuctionConsentedDebuggingDelegate::Create(web_contents);
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            network::switches::kTestThirdPartyCookiePhaseout)) {
      TestThirdPartyCookiePhaseoutInfoBarDelegate::Create(web_contents);
    }
  }

  // Do not show any other info bars in Kiosk mode, because it's unlikely that
  // the viewer can act upon or dismiss them.
  if (IsKioskModeEnabled()) {
    return;
  }

  // Web apps should not display the session restore bubble (crbug.com/1264121)
  if (!is_web_app && HasPendingUncleanExit(browser->profile()))
    SessionCrashedBubble::ShowIfNotOffTheRecordProfile(
        browser, /*skip_tab_checking=*/false);

  // These info bars are not shown when the browser is being controlled by
  // automated tests, so that they don't interfere with tests that assume no
  // info bars.
  if (!startup_command_line.HasSwitch(switches::kTestType) &&
      !IsAutomationEnabled()) {
    // The below info bars are only added to the first profile which is
    // launched. Other profiles might be restoring the browsing sessions
    // asynchronously, so we cannot add the info bars to the focused tabs here.
    //
    // We cannot use `chrome::startup::IsProcessStartup` to determine whether
    // this is the first profile that launched: The browser may be started
    // without a startup window (`kNoStartupWindow`), or open the profile
    // picker, which means that `chrome::startup::IsProcessStartup` will already
    // be `kNo` when the first browser window is opened.
    static bool infobars_shown = false;
    if (infobars_shown) {
      return;
    }
    infobars_shown = true;

    if (show_bad_flags_security_warnings) {
      ShowBadFlagsPrompt(web_contents);
    }

    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);

    if (!google_apis::HasAPIKeyConfigured()) {
      GoogleApiKeysInfoBarDelegate::Create(infobar_manager);
    }

    if (ObsoleteSystem::IsObsoleteNowOrSoon()) {
      PrefService* local_state = g_browser_process->local_state();
      if (!local_state ||
          !local_state->GetBoolean(prefs::kSuppressUnsupportedOSWarning)) {
        ObsoleteSystemInfoBarDelegate::Create(infobar_manager);
      }
    }

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
    if (!is_web_app &&
        !startup_command_line.HasSwitch(switches::kNoDefaultBrowserCheck)) {
      // The default browser prompt should only be shown after the first run.
      if (is_first_run == chrome::startup::IsFirstRun::kNo) {
        ShowDefaultBrowserPrompt(profile);
      }
    }
#endif
  }
}
