// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/check_updater_health_task.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/win_constants.h"
#endif

namespace updater {

CheckUpdaterHealthTask::CheckUpdaterHealthTask(UpdaterScope scope)
    : scope_(scope) {}
CheckUpdaterHealthTask::~CheckUpdaterHealthTask() = default;

void CheckUpdaterHealthTask::CheckAndRecordUpdaterHealth(
    const base::Version& version) {
  base::UmaHistogramBoolean("GoogleUpdate.UpdaterHealth.UpdaterValid",
                            version.IsValid());
  if (!version.IsValid()) {
    return;
  }

#if BUILDFLAG(IS_WIN)
  // System service metrics.
  if (IsSystemInstall(scope_)) {
    for (const bool is_internal_service : {false, true}) {
      const std::wstring service_name =
          GetServiceName(is_internal_service, version);
      const std::string_view uma_suffix =
          is_internal_service ? "Internal" : "SxS";
      base::UmaHistogramBoolean(
          base::StrCat(
              {"GoogleUpdate.UpdaterHealth.ServicePresent.", uma_suffix}),
          IsServicePresent(service_name));
      base::UmaHistogramBoolean(
          base::StrCat(
              {"GoogleUpdate.UpdaterHealth.ServiceEnabled.", uma_suffix}),
          IsServiceEnabled(service_name));
    }
  }

  if (IsSystemInstall(scope_) && !::IsUserAnAdmin()) {
    // When run at medium integrity, the task scheduler interfaces do not
    // enumerate the system `updater` tasks, or in general, any tasks installed
    // by an administrator. Since reliable metrics cannot be gathered on the
    // scheduled tasks under these conditions, metrics are not recorded for
    // this scenario.
    return;
  }

  // Scheduled task metrics.
  scoped_refptr<TaskScheduler> task_scheduler =
      TaskScheduler::CreateInstance(scope_);
  if (!task_scheduler) {
    // Cannot get metrics without a TaskScheduler instance.
    return;
  }
  const std::wstring task_name =
      task_scheduler->FindFirstTaskName(GetTaskNamePrefix(scope_, version));

  // Count the number of tasks for the product.
  size_t number_of_tasks = 0;
  task_scheduler->ForEachTaskWithPrefix(
      base::UTF8ToWide(PRODUCT_FULLNAME_STRING),
      [&](const std::wstring& task_name) { ++number_of_tasks; });

  base::UmaHistogramBoolean("GoogleUpdate.UpdaterHealth.ScheduledTaskPresent",
                            !task_name.empty());
  if (!task_name.empty()) {
    base::UmaHistogramBoolean("GoogleUpdate.UpdaterHealth.ScheduledTaskEnabled",
                              task_scheduler->IsTaskEnabled(task_name));
  }
  base::UmaHistogramCounts100("GoogleUpdate.UpdaterHealth.ScheduledTaskCount",
                              number_of_tasks);
#endif
}

void CheckUpdaterHealthTask::Run(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BrowserUpdaterClient::Create(scope_)->GetUpdaterVersion(
      base::BindPostTask(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          base::BindOnce(&CheckUpdaterHealthTask::CheckAndRecordUpdaterHealth,
                         this))
          .Then(std::move(callback)));
}

}  // namespace updater
