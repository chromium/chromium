// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_HATS_H_
#define CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_HATS_H_

#include <array>
#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_view.h"

class BrowserWindowInterface;
class Profile;

namespace scheduled_restart {

// HaTS Product Specific Data (PSD) field names for Scheduled Restart:
// LINT.IfChange(ScheduledRestartHatsPsdFields)
inline constexpr char kHatsScheduledRestartIncognitoPresentBit[] =
    "Incognito window present";
inline constexpr char kHatsScheduledRestartClientChannel[] = "Client channel";
inline constexpr char kHatsScheduledRestartTimePromptVisible[] =
    "Seconds the prompt was visible";
inline constexpr char kHatsScheduledRestartUserChoice[] = "User choice";
inline constexpr char kHatsScheduledRestartTimeSinceUpdate[] =
    "Days since update became available";
inline constexpr char kHatsScheduledRestartTabsAndWindows[] =
    "Number of open tabs and windows";
inline constexpr char kHatsScheduledRestartTriggerType[] = "Trigger type";
inline constexpr char kHatsScheduledRestartFirstNudgeDelay[] =
    "First nudge delay in days";
inline constexpr char kHatsScheduledRestartNudgeCooldown[] =
    "Nudge cooldown in days";

inline constexpr auto kHatsScheduledRestartStringFields =
    std::to_array<std::string_view>({
        kHatsScheduledRestartClientChannel,
        kHatsScheduledRestartTimePromptVisible,
        kHatsScheduledRestartUserChoice,
        kHatsScheduledRestartTimeSinceUpdate,
        kHatsScheduledRestartTabsAndWindows,
        kHatsScheduledRestartTriggerType,
        kHatsScheduledRestartFirstNudgeDelay,
        kHatsScheduledRestartNudgeCooldown,
    });
// LINT.ThenChange(//chrome/browser/ui/hats/survey_config.cc:ScheduledRestartHatsPsdFields)

// LocalState dictionary keys for post-relaunch HaTS survey persistence:
inline constexpr char kHatsScheduledRestartPendingTimestampKey[] = "timestamp";
inline constexpr char kHatsScheduledRestartPendingIncognitoKey[] =
    "incognito_present";
inline constexpr char kHatsScheduledRestartPendingStringDataKey[] =
    "string_data";

// HaTS survey launch delay for Scheduled Restart. Set to 5 seconds so the
// "restart when idle" confirmation toast finishes dismissing before the survey
// bubble appears (must be longer than ToastController::kToastDefaultTimeout,
// enforced by a static_assert in the .cc).
inline constexpr base::TimeDelta kHatsScheduledRestartSurveyDelay =
    base::Seconds(5);

// Evaluates and launches (or persists across relaunch for kRestartNow) the
// Scheduled Restart HaTS survey for `choice`.
void MaybeLaunchHatsSurvey(
    ScheduledRestartBubbleView::ScheduledRestartDialogChoice choice,
    BrowserWindowInterface* browser,
    bool show_active,
    base::TimeDelta duration);

// Consumes and launches any pending post-relaunch HaTS survey for `profile`
// following session restoration.
void MaybeLaunchPendingHatsSurvey(Profile* profile);

}  // namespace scheduled_restart

#endif  // CHROME_BROWSER_UI_VIEWS_SCHEDULED_RESTART_SCHEDULED_RESTART_HATS_H_
