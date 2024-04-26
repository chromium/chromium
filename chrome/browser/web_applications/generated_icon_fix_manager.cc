// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/generated_icon_fix_manager.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

namespace {

bool g_disable_auto_retry_for_testing = false;

bool IsEnabled() {
  return base::FeatureList::IsEnabled(
      features::kWebAppSyncGeneratedIconBackgroundFix);
}

}  // namespace

// static
void GeneratedIconFixManager::DisableAutoRetryForTesting() {
  g_disable_auto_retry_for_testing = true;
}

GeneratedIconFixManager::GeneratedIconFixManager() = default;

GeneratedIconFixManager::~GeneratedIconFixManager() = default;

void GeneratedIconFixManager::SetProvider(base::PassKey<WebAppProvider>,
                                          WebAppProvider& provider) {
  provider_ = &provider;
}

void GeneratedIconFixManager::Start() {
  if (!IsEnabled()) {
    return;
  }

  provider_->scheduler().ScheduleCallback(
      "GeneratedIconFixManager::Start", AllAppsLockDescription(),
      base::BindOnce(&GeneratedIconFixManager::ScheduleFixes,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void GeneratedIconFixManager::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void GeneratedIconFixManager::ScheduleFixes(AllAppsLock& all_apps_lock,
                                            base::Value::Dict& debug_value) {
  int started_attempt_count = 0;
  for (const webapps::AppId& app_id : all_apps_lock.registrar().GetAppIds()) {
    bool scheduled = MaybeScheduleFix(app_id, all_apps_lock,
                                      *debug_value.EnsureDict(app_id));
    if (!scheduled) {
      continue;
    }
    debug_value.EnsureList("fixes_scheduled")->Append(app_id);

    ++started_attempt_count;
    const WebApp* app = all_apps_lock.registrar().GetAppById(app_id);
    const std::optional<GeneratedIconFix>& generated_icon_fix =
        app->generated_icon_fix();
    base::UmaHistogramCounts100("WebApp.GeneratedIconFix.AttemptCount",
                                generated_icon_fix.has_value()
                                    ? generated_icon_fix->attempt_count()
                                    : 0u);
  }
  base::UmaHistogramCounts100("WebApp.GeneratedIconFix.StartUpAttemptCount",
                              started_attempt_count);
  debug_value.Set("started_attempt_count", started_attempt_count);
}

bool GeneratedIconFixManager::MaybeScheduleFix(const webapps::AppId& app_id,
                                               WithAppResources& resources,
                                               base::Value::Dict& debug_value) {
  CHECK(IsEnabled());

  const WebApp* app = resources.registrar().GetAppById(app_id);
  GeneratedIconFixScheduleDecision decision = MakeScheduleDecision(app);

  debug_value.Set("decision", base::ToString(decision));
  base::UmaHistogramEnumeration("WebApp.GeneratedIconFix.ScheduleDecision",
                                decision);

  bool schedule = decision == GeneratedIconFixScheduleDecision::kSchedule;
  debug_value.Set("schedule", schedule);
  if (schedule) {
    scheduled_fixes_.insert(app_id);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GeneratedIconFixManager::StartFix,
                       weak_ptr_factory_.GetWeakPtr(), app_id),
        generated_icon_fix_util::GetThrottleDuration(*app));
  }

  if (maybe_schedule_callback_for_testing_) {
    std::move(maybe_schedule_callback_for_testing_).Run(app_id, decision);
  }

  return schedule;
}

void GeneratedIconFixManager::MaybeScheduleFixAppLock(
    const webapps::AppId& app_id,
    AppLock& app_lock,
    base::Value::Dict& debug_value) {
  MaybeScheduleFix(app_id, app_lock, debug_value);
}

GeneratedIconFixScheduleDecision GeneratedIconFixManager::MakeScheduleDecision(
    const WebApp* app) {
  if (!app || !app->IsSynced()) {
    return GeneratedIconFixScheduleDecision::kNotSynced;
  }

  if (!app->is_generated_icon()) {
    // TODO(crbug.com/40185008): Check for icon bitmaps that match the generated
    // icon bitmap for users that were affected by crbug.com/1317922.
    return GeneratedIconFixScheduleDecision::kNotRequired;
  }

  if (!generated_icon_fix_util::HasRemainingAttempts(*app)) {
    return GeneratedIconFixScheduleDecision::kAttemptLimitReached;
  }

  if (!generated_icon_fix_util::IsWithinFixTimeWindow(*app)) {
    return GeneratedIconFixScheduleDecision::kTimeWindowExpired;
  }

  return scheduled_fixes_.contains(app->app_id())
             ? GeneratedIconFixScheduleDecision::kAlreadyScheduled
             : GeneratedIconFixScheduleDecision::kSchedule;
}

void GeneratedIconFixManager::StartFix(const webapps::AppId& app_id) {
  provider_->command_manager().ScheduleCommand(
      std::make_unique<GeneratedIconFixCommand>(
          app_id, GeneratedIconFixSource_RETROACTIVE,
          base::BindOnce(&GeneratedIconFixManager::FixCompleted,
                         weak_ptr_factory_.GetWeakPtr(), app_id)));
}

void GeneratedIconFixManager::FixCompleted(const webapps::AppId& app_id,
                                           GeneratedIconFixResult result) {
  base::UmaHistogramEnumeration("WebApp.GeneratedIconFix.Result", result);

  CHECK_EQ(scheduled_fixes_.erase(app_id), 1u);

  // Retry on failure.
  switch (result) {
    case GeneratedIconFixResult::kAppUninstalled:
    case GeneratedIconFixResult::kShutdown:
    case GeneratedIconFixResult::kSuccess:
      break;
    case GeneratedIconFixResult::kDownloadFailure:
    case GeneratedIconFixResult::kStillGenerated:
    case GeneratedIconFixResult::kWriteFailure: {
      if (!g_disable_auto_retry_for_testing) {
        provider_->scheduler().ScheduleCallback(
            "GeneratedIconFixManager::Retry", AppLockDescription(app_id),
            base::BindOnce(&GeneratedIconFixManager::MaybeScheduleFixAppLock,
                           weak_ptr_factory_.GetWeakPtr(), app_id),
            base::DoNothing());
      }
      break;
    }
  }

  if (fix_completed_callback_for_testing_) {
    std::move(fix_completed_callback_for_testing_).Run(app_id, result);
  }
}

}  // namespace web_app
