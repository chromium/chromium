// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace whats_new {
bool g_is_remote_content_disabled = false;

#if !BUILDFLAG(IS_CHROMEOS)
// For testing purposes, so that WebUI tests run on non-branded
// CQ bots.
BASE_FEATURE(kForceEnabled,
             "WhatsNewForceEnabled",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool IsEnabled() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
  return true;
#elif !BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(whats_new::kForceEnabled);
#else
  return false;
#endif
}

void DisableRemoteContentForTests() {
  g_is_remote_content_disabled = true;
}

void LogStartupType(StartupType type) {
#if !BUILDFLAG(IS_CHROMEOS)
  base::UmaHistogramEnumeration("WhatsNew.StartupType", type);
#endif
}

bool IsRemoteContentDisabled() {
  return g_is_remote_content_disabled;
}

bool ShouldShowForState(PrefService* local_state,
                        bool promotional_tabs_enabled) {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  LogStartupType(StartupType::kCalledShouldShow);

  if (!promotional_tabs_enabled) {
    whats_new::LogStartupType(whats_new::StartupType::kPromotionalTabsDisabled);
    return false;
  }

  if (!local_state ||
      !local_state->FindPreference(prefs::kLastWhatsNewVersion)) {
    LogStartupType(StartupType::kInvalidState);
    return false;
  }

  // Allow disabling the What's New experience in tests using the standard
  // kNoFirstRun switch. This behavior can be overridden using the
  // kForceWhatsNew switch for the What's New experience integration tests.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if ((command_line->HasSwitch(switches::kNoFirstRun) &&
       !command_line->HasSwitch(switches::kForceWhatsNew)) ||
      !IsEnabled()) {
    LogStartupType(StartupType::kFeatureDisabled);
    return false;
  }

  int last_version = local_state->GetInteger(prefs::kLastWhatsNewVersion);

  // Don't show What's New if it's already been shown for the current major
  // milestone.
  if (CHROME_VERSION_MAJOR <= last_version) {
    LogStartupType(StartupType::kAlreadyShown);
    return false;
  }

  // Set the last version here to indicate that What's New should not attempt
  // to display again for this milestone. This prevents the page from
  // potentially displaying multiple times in a given milestone, e.g. for
  // multiple profile relaunches (see https://crbug.com/1274313).
  local_state->SetInteger(prefs::kLastWhatsNewVersion, CHROME_VERSION_MAJOR);
  return true;
#endif
}

GURL GetWebUIStartupURL() {
#if !BUILDFLAG(IS_CHROMEOS)
  return net::AppendQueryParameter(GURL(chrome::kChromeUIWhatsNewURL), "auto",
                                   "true");
#else
  NOTREACHED();
#endif
}

}  // namespace whats_new
