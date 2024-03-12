// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/chrome_content_browser_client_isolated_web_apps_part.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/pref_names.h"
#endif

namespace web_apps {

ChromeContentBrowserClientIsolatedWebAppsPart::
    ChromeContentBrowserClientIsolatedWebAppsPart() = default;
ChromeContentBrowserClientIsolatedWebAppsPart::
    ~ChromeContentBrowserClientIsolatedWebAppsPart() = default;

// static
bool ChromeContentBrowserClientIsolatedWebAppsPart::AreIsolatedWebAppsEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!web_app::AreWebAppsEnabled(profile)) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Check if the enterprise policy that regulates Isolated Web Apps force
  // installing is present. If it is there then the IWAs should be enabled.
  const base::Value::List& isolated_web_apps =
      profile->GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList);
  if (!isolated_web_apps.empty()) {
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // IWAs should be enabled for ShimlessRMA app profile.
  if (ash::IsShimlessRmaAppBrowserContext(browser_context)) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return true;
  }

  return false;
}

void ChromeContentBrowserClientIsolatedWebAppsPart::
    AppendExtraRendererCommandLineSwitches(
        base::CommandLine* command_line,
        content::RenderProcessHost& process) {
  if (!content::IsolatedWebAppsPolicy::AreIsolatedWebAppsEnabled(
          process.GetBrowserContext())) {
    return;
  }
  command_line->AppendSwitch(switches::kEnableIsolatedWebAppsInRenderer);
  if (process.GetWebExposedIsolationLevel() >=
      content::WebExposedIsolationLevel::kIsolatedApplication) {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "DirectSockets");
  }
}

}  // namespace web_apps
