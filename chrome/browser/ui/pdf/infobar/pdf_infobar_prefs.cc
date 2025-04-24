// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_prefs.h"

#include <cmath>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

void SetInfoBarShownRecently() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(
      prefs::kPdfInfoBarTimesShown,
      local_state->GetInteger(prefs::kPdfInfoBarTimesShown) + 1);
  local_state->SetTime(prefs::kPdfInfoBarLastShown, base::Time::Now());
}

bool InfoBarShownRecentlyOrMaxTimes() {
  PrefService* local_state = g_browser_process->local_state();
  const int times_shown = local_state->GetInteger(prefs::kPdfInfoBarTimesShown);
  if (times_shown == 0) {
    return false;
  }
  if (times_shown >= kPdfInfoBarMaxTimesToShow) {
    return true;
  }
  const double interval_days =
      kPdfInfoBarShowIntervalDays * pow(2, times_shown - 1);
  const base::Time last_shown =
      local_state->GetTime(prefs::kPdfInfoBarLastShown);
  return last_shown + base::Days(interval_days) > base::Time::Now();
}

bool IsPdfViewerDisabled(Profile* profile) {
  auto* prefs = profile->GetPrefs();
  return prefs->HasPrefPath(prefs::kPluginsAlwaysOpenPdfExternally) &&
         prefs->GetBoolean(prefs::kPluginsAlwaysOpenPdfExternally);
}

bool IsDefaultBrowserPolicyControlled() {
  auto* local_state = g_browser_process->local_state();
  return local_state->IsManagedPreference(
             prefs::kDefaultBrowserSettingEnabled) &&
         local_state->GetBoolean(prefs::kDefaultBrowserSettingEnabled);
}
