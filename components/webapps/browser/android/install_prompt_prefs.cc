// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/install_prompt_prefs.h"

#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/features.h"

namespace webapps {

namespace {

constexpr char kInstallPromptDismissCount[] =
    "web_apps.install_prompt.global.consecutive_dismiss";
constexpr char kInstallPromptLastDismissTime[] =
    "web_apps.install_prompt.global.last_dismiss";

constexpr char kInstallPromptIgnoreCount[] =
    "web_apps.install_prompt.global.consecutive_ignore";
constexpr char kInstallPromptLastIgnoreTime[] =
    "web_apps.install_prompt.global.last_ignore";

}  // anonymous namespace

// static
void InstallPromptPrefs::RegisterLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kInstallPromptDismissCount, 0);
  registry->RegisterTimePref(kInstallPromptLastDismissTime, base::Time());
  registry->RegisterIntegerPref(kInstallPromptIgnoreCount, 0);
  registry->RegisterTimePref(kInstallPromptLastIgnoreTime, base::Time());
}

// static
void InstallPromptPrefs::RecordInstallPromptDismissed(PrefService* pref_service,
                                                      base::Time time) {
  int dismiss_count = pref_service->GetInteger(kInstallPromptDismissCount);
  pref_service->SetInteger(kInstallPromptDismissCount, dismiss_count + 1);
  pref_service->SetTime(kInstallPromptLastDismissTime, time);
}

// static
void InstallPromptPrefs::RecordInstallPromptIgnored(PrefService* pref_service,
                                                    base::Time time) {
  int ignored_count = pref_service->GetInteger(kInstallPromptIgnoreCount);
  pref_service->SetInteger(kInstallPromptIgnoreCount, ignored_count + 1);
  pref_service->SetTime(kInstallPromptLastIgnoreTime, time);
}

// static
void InstallPromptPrefs::RecordInstallPromptClicked(PrefService* pref_service) {
  // Reset dismiss and ignore count.
  pref_service->SetInteger(kInstallPromptDismissCount, 0);
  pref_service->SetInteger(kInstallPromptIgnoreCount, 0);
}

// static
bool InstallPromptPrefs::IsPromptDismissedConsecutivelyRecently(
    PrefService* pref_service,
    base::Time now) {
  if (!base::FeatureList::IsEnabled(features::kInstallPromptGlobalGuardrails)) {
    return false;
  }

  int dismiss_count = pref_service->GetInteger(kInstallPromptDismissCount);
  if (dismiss_count >=
      features::kInstallPromptGlobalGuardrails_DismissCount.Get()) {
    base::Time last_dismiss_time =
        pref_service->GetTime(kInstallPromptLastDismissTime);

    return (now - last_dismiss_time <
            features::kInstallPromptGlobalGuardrails_DismissPeriod.Get());
  }
  return false;
}

// static
bool InstallPromptPrefs::IsPromptIgnoredConsecutivelyRecently(
    PrefService* pref_service,
    base::Time now) {
  if (!base::FeatureList::IsEnabled(features::kInstallPromptGlobalGuardrails)) {
    return false;
  }

  int ignore_count = pref_service->GetInteger(kInstallPromptIgnoreCount);
  if (ignore_count >=
      features::kInstallPromptGlobalGuardrails_IgnoreCount.Get()) {
    base::Time last_ignore_time =
        pref_service->GetTime(kInstallPromptLastIgnoreTime);

    return (now - last_ignore_time <
            features::kInstallPromptGlobalGuardrails_IgnorePeriod.Get());
  }
  return false;
}

}  // namespace webapps
