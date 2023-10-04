// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/generated_icon_fix_manager.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
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

  provider_->scheduler().ScheduleCallbackWithLock<AllAppsLock>(
      "GeneratedIconFixManager::Start",
      std::make_unique<AllAppsLockDescription>(),
      base::BindOnce(
          [](base::WeakPtr<GeneratedIconFixManager> manager,
             AllAppsLock& all_apps_lock) {
            if (!manager) {
              return;
            }
            size_t started_attempt_count = 0;
            for (const webapps::AppId& app_id :
                 all_apps_lock.registrar().GetAppIds()) {
              bool scheduled = manager->MaybeScheduleFix(all_apps_lock, app_id);
              if (!scheduled) {
                continue;
              }

              ++started_attempt_count;
              const WebApp* app = all_apps_lock.registrar().GetAppById(app_id);
              const absl::optional<GeneratedIconFix>& generated_icon_fix =
                  app->generated_icon_fix();
              base::UmaHistogramCounts100(
                  "WebApp.GeneratedIconFix.AttemptCount",
                  generated_icon_fix.has_value()
                      ? generated_icon_fix->attempt_count()
                      : 0u);
            }
            base::UmaHistogramCounts100(
                "WebApp.GeneratedIconFix.StartUpAttemptCount",
                started_attempt_count);
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void GeneratedIconFixManager::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool GeneratedIconFixManager::MaybeScheduleFix(WithAppResources& resources,
                                               const webapps::AppId& app_id) {
  CHECK(IsEnabled());

  const WebApp* app = resources.registrar().GetAppById(app_id);
  GeneratedIconFixScheduleDecision decision = MakeScheduleDecision(app);
  base::UmaHistogramEnumeration("WebApp.GeneratedIconFix.ScheduleDecision",
                                decision);

  bool schedule = decision == GeneratedIconFixScheduleDecision::kSchedule;
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

GeneratedIconFixScheduleDecision GeneratedIconFixManager::MakeScheduleDecision(
    const WebApp* app) {
  if (!app || !app->IsSynced()) {
    return GeneratedIconFixScheduleDecision::kNotSynced;
  }

  if (!app->is_generated_icon()) {
    // TODO(crbug.com/1216965): Check for icon bitmaps that match the generated
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
        provider_->scheduler().ScheduleCallbackWithLock<AppLock>(
            "GeneratedIconFixManager::Retry",
            std::make_unique<AppLockDescription>(app_id),
            base::BindOnce(
                [](base::WeakPtr<GeneratedIconFixManager> manager,
                   const webapps::AppId& app_id, AppLock& app_lock) {
                  if (!manager) {
                    return;
                  }
                  manager->MaybeScheduleFix(app_lock, app_id);
                },
                weak_ptr_factory_.GetWeakPtr(), app_id));
      }
      break;
    }
  }

  if (fix_completed_callback_for_testing_) {
    std::move(fix_completed_callback_for_testing_).Run(app_id, result);
  }
}

}  // namespace web_app
