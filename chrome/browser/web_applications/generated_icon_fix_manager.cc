// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/generated_icon_fix_manager.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"

namespace web_app {

namespace {

constexpr base::TimeDelta kFixWindowDuration = base::Days(7);

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

GeneratedIconFixScheduleDecision GeneratedIconFixManager::MaybeScheduleFix(
    WithAppResources& resources,
    const webapps::AppId& app_id) {
  CHECK(IsEnabled());

  GeneratedIconFixScheduleDecision decision =
      MakeScheduleDecision(resources.registrar(), app_id);

  if (decision == GeneratedIconFixScheduleDecision::kSchedule) {
    scheduled_fixes_.insert(app_id);
    provider_->command_manager().ScheduleCommand(
        std::make_unique<GeneratedIconFixCommand>(
            app_id, base::BindOnce(&GeneratedIconFixManager::FixCompleted,
                                   weak_ptr_factory_.GetWeakPtr(), app_id)));
  }

  if (maybe_schedule_callback_for_testing_) {
    std::move(maybe_schedule_callback_for_testing_).Run(app_id, decision);
  }

  return decision;
}

GeneratedIconFixScheduleDecision GeneratedIconFixManager::MakeScheduleDecision(
    const WebAppRegistrar& registrar,
    const webapps::AppId& app_id) {
  const WebApp* app = registrar.GetAppById(app_id);
  if (!app || !app->IsSynced()) {
    return GeneratedIconFixScheduleDecision::kNoApp;
  }

  base::TimeDelta duration_since_installation =
      time_for_testing_.value_or(base::Time::Now()) - app->first_install_time();
  if (duration_since_installation > kFixWindowDuration) {
    // TODO(crbug.com/1216965): Enable a one off retroactive fix for
    // pre-existing generated icons from before this fix was added (effectively
    // reset their time window).
    return GeneratedIconFixScheduleDecision::kTimeWindowExpired;
  }

  if (!app->is_generated_icon()) {
    // TODO(crbug.com/1216965): Check for icon bitmaps that match the generated
    // icon bitmap for users that were affected by crbug.com/1317922.
    return GeneratedIconFixScheduleDecision::kNotRequired;
  }

  // TODO(crbug.com/1216965): Throttle fix attempts to once per day.

  return scheduled_fixes_.contains(app_id)
             ? GeneratedIconFixScheduleDecision::kAlreadyScheduled
             : GeneratedIconFixScheduleDecision::kSchedule;
}

void GeneratedIconFixManager::FixCompleted(const webapps::AppId& app_id,
                                           GeneratedIconFixResult result) {
  CHECK_EQ(scheduled_fixes_.erase(app_id), 1u);

  if (fix_completed_callback_for_testing_) {
    std::move(fix_completed_callback_for_testing_).Run(app_id, result);
  }
}

}  // namespace web_app
