// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/generated_icon_fix_manager.h"

#include "base/feature_list.h"
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

bool IsEnabled() {
  return base::FeatureList::IsEnabled(
      features::kWebAppSyncGeneratedIconBackgroundFix);
}

}  // namespace

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
            for (const webapps::AppId& app_id :
                 all_apps_lock.registrar().GetAppIds()) {
              manager->MaybeScheduleFix(all_apps_lock, app_id);
            }
            // TODO(crbug.com/1216965): Record the count of how many fixes were
            // scheduled.
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void GeneratedIconFixManager::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

GeneratedIconFixScheduleDecision GeneratedIconFixManager::MaybeScheduleFix(
    WithAppResources& resources,
    const webapps::AppId& app_id) {
  CHECK(IsEnabled());

  const WebApp* app = resources.registrar().GetAppById(app_id);
  GeneratedIconFixScheduleDecision decision = MakeScheduleDecision(app);

  if (decision == GeneratedIconFixScheduleDecision::kSchedule) {
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

  return decision;
}

GeneratedIconFixScheduleDecision GeneratedIconFixManager::MakeScheduleDecision(
    const WebApp* app) {
  if (!app || !app->IsSynced()) {
    return GeneratedIconFixScheduleDecision::kNoApp;
  }

  if (!generated_icon_fix_util::HasRemainingAttempts(*app)) {
    return GeneratedIconFixScheduleDecision::kAttemptLimitReached;
  }

  if (!generated_icon_fix_util::IsWithinFixTimeWindow(*app)) {
    return GeneratedIconFixScheduleDecision::kTimeWindowExpired;
  }

  if (!app->is_generated_icon()) {
    // TODO(crbug.com/1216965): Check for icon bitmaps that match the generated
    // icon bitmap for users that were affected by crbug.com/1317922.
    return GeneratedIconFixScheduleDecision::kNotRequired;
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
                // TODO(crbug.com/1216965): Record this retry attempt.
              },
              weak_ptr_factory_.GetWeakPtr(), app_id));
      break;
    }
  };

  if (fix_completed_callback_for_testing_) {
    std::move(fix_completed_callback_for_testing_).Run(app_id, result);
  }
}

}  // namespace web_app
