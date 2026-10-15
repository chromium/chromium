// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_hats.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/version_info/version_info.h"
#include "services/metrics/public/cpp/metrics_utils.h"

namespace scheduled_restart {

// Ensure the survey launch delay allows the "Restart when idle" confirmation
// toast to finish dismissing first so the two UI surfaces do not collide.
static_assert(kHatsScheduledRestartSurveyDelay >
              ToastController::kToastDefaultTimeout);

namespace {

// A pending survey older than this is discarded. If a relaunch is aborted
// (e.g. by a beforeunload handler or cancelled download prompt), the pending
// pref remains until expired or consumed. Any session restore occurring within
// this 10-minute window (such as opening another profile) could otherwise
// trigger the post-relaunch survey; this cap bounds how long a stale pending
// survey can persist.
constexpr base::TimeDelta kPendingHatsSurveyMaxAge = base::Minutes(10);

struct BrowserStats {
  int tabs = 0;
  int windows = 0;
  bool incognito_present = false;
};

// Counts open tabs and windows across all non-OTR normal browser windows so
// DevTools, popups, app windows, and Incognito/Guest windows do not inflate the
// regular tab load. Also checks whether any incognito browser window is open
// (excluding DevTools and Guest).
BrowserStats CountNormalBrowsers() {
  BrowserStats stats;
  if (auto* collection = GlobalBrowserCollection::GetInstance()) {
    collection->ForEach([&](BrowserWindowInterface* browser_interface) {
      if (browser_interface->GetType() !=
          BrowserWindowInterface::Type::TYPE_NORMAL) {
        return true;
      }
      if (browser_interface->GetProfile()->IsOffTheRecord()) {
        return true;
      }
      stats.windows++;
      if (auto* tab_strip = browser_interface->GetTabStripModel()) {
        stats.tabs += tab_strip->count();
      }
      return true;
    });
    stats.incognito_present = collection->GetIncognitoBrowserCount() > 0;
  }
  return stats;
}

// Days since update detection is capped at 90 days (~1 quarter / ~6 milestone
// cycles) for data minimization and anti-fingerprinting while preserving
// fidelity across repeated nudge cycles.
constexpr int kMaxDaysSinceUpdate = 90;

int GetDaysSinceUpdate() {
  auto* srm =
      g_browser_process && g_browser_process->GetFeatures()
          ? g_browser_process->GetFeatures()->scheduled_restart_manager()
          : nullptr;
  UpgradeDetector* upgrade_detector =
      srm ? srm->upgrade_detector() : UpgradeDetector::GetInstance();
  if (!upgrade_detector) {
    return 0;
  }
  base::Time detected_time = upgrade_detector->upgrade_detected_time();
  if (detected_time.is_null()) {
    return 0;
  }
  return std::clamp(
      static_cast<int>((base::Time::Now() - detected_time).InDays()), 0,
      kMaxDaysSinceUpdate);
}

std::string_view GetUserChoicePsd(
    ScheduledRestartBubbleView::ScheduledRestartDialogChoice choice) {
  switch (choice) {
    case ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kRestartNow:
      return "restart now";
    case ScheduledRestartBubbleView::ScheduledRestartDialogChoice::
        kScheduledOnIdle:
      return "restart when idle";
    case ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kDismissed:
      return "dismiss";
  }
  NOTREACHED();
}

void PersistPendingSurvey(bool incognito_present,
                          SurveyStringData string_data) {
  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!local_state) {
    return;
  }

  local_state->SetDict(
      prefs::kScheduledRestartHatsPendingSurvey,
      base::DictValue()
          .Set(kHatsScheduledRestartPendingTimestampKey,
               base::TimeToValue(base::Time::Now()))
          .Set(kHatsScheduledRestartPendingIncognitoKey, incognito_present)
          .Set(kHatsScheduledRestartPendingStringDataKey,
               base::DictValue(std::make_move_iterator(string_data.begin()),
                               std::make_move_iterator(string_data.end()))));
}

void LaunchHatsSurvey(Profile* profile,
                      BrowserWindowInterface* browser,
                      const SurveyBitsData& bits_data,
                      const SurveyStringData& string_data) {
  if (!profile) {
    return;
  }
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);
  if (!hats_service) {
    return;
  }

  const int timeout_ms = kHatsScheduledRestartSurveyDelay.InMilliseconds();
  tabs::TabInterface* active_tab =
      browser ? browser->GetActiveTabInterface() : nullptr;
  if (active_tab && active_tab->GetContents()) {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerScheduledRestart, active_tab->GetContents(),
        timeout_ms, bits_data, string_data,
        HatsService::NavigationBehavior::ALLOW_ANY);
  } else {
    hats_service->LaunchDelayedSurvey(kHatsSurveyTriggerScheduledRestart,
                                      timeout_ms, bits_data, string_data);
  }
}

}  // namespace

void MaybeLaunchHatsSurvey(
    ScheduledRestartBubbleView::ScheduledRestartDialogChoice choice,
    BrowserWindowInterface* browser,
    bool show_active,
    base::TimeDelta duration) {
  // Clients outside the HaTS rollout skip all PSD collection, keyed
  // service creation, and LocalState writes.
  if (!base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopScheduledRestart)) {
    return;
  }
  // Do not create keyed services or schedule surveys while Chrome is
  // shutting down.
  if (browser_shutdown::HasShutdownStarted()) {
    return;
  }
  if (!browser || browser->IsDeleteScheduled()) {
    return;
  }
  // Off-the-record profiles (e.g. Incognito and Guest) never participate in
  // HaTS surveys.
  Profile* profile = browser->GetProfile();
  CHECK(profile);
  if (profile->IsOffTheRecord()) {
    return;
  }

  const BrowserStats stats = CountNormalBrowsers();
  // `show_active` maps to whether the dialog was triggered manually from the
  // App Menu (active) or automatically as a passive NTP nudge (inactive).
  SurveyStringData string_data = {
      {kHatsScheduledRestartClientChannel,
       std::string(version_info::GetChannelString(chrome::GetChannel()))},
      {kHatsScheduledRestartTimePromptVisible,
       base::NumberToString(
           ukm::GetExponentialBucketMinForUserTiming(duration.InSeconds()))},
      {kHatsScheduledRestartUserChoice, std::string(GetUserChoicePsd(choice))},
      {kHatsScheduledRestartTimeSinceUpdate,
       base::NumberToString(GetDaysSinceUpdate())},
      {kHatsScheduledRestartTabsAndWindows,
       base::StringPrintf("%d tabs, %d windows", stats.tabs, stats.windows)},
      {kHatsScheduledRestartTriggerType, show_active ? "manual" : "automatic"},
      {kHatsScheduledRestartFirstNudgeDelay,
       base::NumberToString(
           ScheduledRestartManager::GetFirstNudgeDelay().InDays())},
      {kHatsScheduledRestartNudgeCooldown,
       base::NumberToString(
           ScheduledRestartManager::GetNudgeCooldown().InDays())},
  };

  // Immediate relaunch terminates the browser process before in-session HaTS
  // triggers can deliver. Persist survey parameters to LocalState; the survey
  // will launch upon SessionRestore in the new session.
  if (choice ==
      ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kRestartNow) {
    PersistPendingSurvey(stats.incognito_present, std::move(string_data));
    return;
  }

  SurveyBitsData bits_data = {
      {kHatsScheduledRestartIncognitoPresentBit, stats.incognito_present},
  };
  LaunchHatsSurvey(profile, browser, bits_data, string_data);
}

void MaybeLaunchPendingHatsSurvey(Profile* profile) {
  if (!profile || profile->IsOffTheRecord() ||
      browser_shutdown::HasShutdownStarted()) {
    return;
  }

  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!local_state) {
    return;
  }

  const base::DictValue& pending_ref =
      local_state->GetDict(prefs::kScheduledRestartHatsPendingSurvey);
  if (pending_ref.empty()) {
    return;
  }

  // Clone and clear the pref immediately so the survey is attempted at most
  // once (delivering to whichever regular profile completes session restore
  // first).
  base::DictValue pending = pending_ref.Clone();
  local_state->ClearPref(prefs::kScheduledRestartHatsPendingSurvey);

  if (!base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopScheduledRestart)) {
    return;
  }

  std::optional<base::Time> timestamp =
      base::ValueToTime(pending.Find(kHatsScheduledRestartPendingTimestampKey));
  if (!timestamp) {
    return;
  }
  const base::TimeDelta age = base::Time::Now() - *timestamp;
  if (age.is_negative() || age > kPendingHatsSurveyMaxAge) {
    return;
  }

  const base::DictValue* string_dict =
      pending.FindDict(kHatsScheduledRestartPendingStringDataKey);
  if (!string_dict) {
    return;
  }

  // Reconstruct string_data from the expected registered fields rather than
  // iterating raw dictionary keys so that any schema change across a Chrome
  // version upgrade cannot trigger a CHECK failure in HatsServiceDesktop.
  SurveyStringData string_data;
  for (std::string_view field : kHatsScheduledRestartStringFields) {
    const std::string* val = string_dict->FindString(field);
    if (!val) {
      return;
    }
    string_data.emplace(field, *val);
  }

  SurveyBitsData bits_data = {
      {kHatsScheduledRestartIncognitoPresentBit,
       pending.FindBool(kHatsScheduledRestartPendingIncognitoKey)
           .value_or(false)},
  };

  BrowserWindowInterface* browser = nullptr;
  if (auto* collection = ProfileBrowserCollection::GetForProfile(profile)) {
    browser = collection->GetLastActiveBrowser();
  }

  LaunchHatsSurvey(profile, browser, bits_data, string_data);
}

}  // namespace scheduled_restart
