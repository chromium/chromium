// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_GENERATED_ICON_FIX_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_GENERATED_ICON_FIX_MANAGER_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/generated_icon_fix_command.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

class AllAppsLock;
class AppLock;
class WebApp;
class WebAppProvider;
class WithAppResources;

// Used by metrics.
enum class GeneratedIconFixScheduleDecision {
  kNotSynced = 0,
  kTimeWindowExpired = 1,
  kNotRequired = 2,
  kAttemptLimitReached = 3,
  kAlreadyScheduled = 4,
  kSchedule = 5,

  kMaxValue = kSchedule,
};

class GeneratedIconFixManager {
 public:
  static void DisableAutoRetryForTesting();

  GeneratedIconFixManager();
  ~GeneratedIconFixManager();

  void SetProvider(base::PassKey<WebAppProvider>, WebAppProvider& provider);
  void Start();

  // TODO(crbug.com/40185008): Schedule fixes ten minutes after sync install.
  // TODO(crbug.com/40185008): Schedule fixes on network reconnection.

  void InvalidateWeakPtrsForTesting();

  base::flat_set<webapps::AppId>& scheduled_fixes_for_testing() {
    return scheduled_fixes_;
  }

  base::OnceCallback<void(const webapps::AppId&,
                          GeneratedIconFixScheduleDecision)>&
  maybe_schedule_callback_for_testing() {
    return maybe_schedule_callback_for_testing_;
  }

  base::OnceCallback<void(const webapps::AppId&, GeneratedIconFixResult)>&
  fix_completed_callback_for_testing() {
    return fix_completed_callback_for_testing_;
  }

 private:
  void ScheduleFixes(AllAppsLock& all_apps_lock,
                     base::Value::Dict& debug_value);

  // Returns whether a fix was newly scheduled for `app_id`.
  bool MaybeScheduleFix(const webapps::AppId& app_id,
                        WithAppResources& resources,
                        base::Value::Dict& debug_value);
  // Separate method that simply calls the above one, required due to
  // templating.
  void MaybeScheduleFixAppLock(const webapps::AppId& app_id,
                               AppLock& app_lock,
                               base::Value::Dict& debug_value);

  GeneratedIconFixScheduleDecision MakeScheduleDecision(const WebApp* app);
  void StartFix(const webapps::AppId& app_id);
  void FixCompleted(const webapps::AppId& app_id,
                    GeneratedIconFixResult result);

  raw_ptr<WebAppProvider> provider_ = nullptr;

  base::flat_set<webapps::AppId> scheduled_fixes_;

  base::OnceCallback<void(const webapps::AppId&,
                          GeneratedIconFixScheduleDecision)>
      maybe_schedule_callback_for_testing_;
  base::OnceCallback<void(const webapps::AppId&, GeneratedIconFixResult)>
      fix_completed_callback_for_testing_;

  base::WeakPtrFactory<GeneratedIconFixManager> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_GENERATED_ICON_FIX_MANAGER_H_
