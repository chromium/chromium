// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/get_isolated_web_app_browsing_data_command.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/commands/computed_app_size.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/jobs/get_isolated_web_app_size_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/storage_partition_config.h"
#include "ui/base/models/tree_model.h"
#include "url/origin.h"

namespace web_app {

GetIsolatedWebAppBrowsingDataCommand::GetIsolatedWebAppBrowsingDataCommand(
    Profile* profile,
    BrowsingDataCallback callback)
    : WebAppCommand<AllAppsLock, base::flat_map<url::Origin, uint64_t>>(
          "GetIsolatedWebAppBrowsingDataCommand",
          AllAppsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/{}),
      profile_(*profile) {}

GetIsolatedWebAppBrowsingDataCommand::~GetIsolatedWebAppBrowsingDataCommand() =
    default;

void GetIsolatedWebAppBrowsingDataCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  lock_ = std::move(lock);

  auto concurrent =
      base::ConcurrentCallbacks<std::optional<ComputedAppSizeWithOrigin>>();
  for (const auto& iwa :
       lock_->registrar().GetApps(WebAppFilter::IsIsolatedApp())) {
    get_isolated_web_app_size_jobs_.push_back(
        std::make_unique<GetIsolatedWebAppSizeJob>(
            &profile_.get(), iwa.app_id(), GetMutableDebugValue(),
            concurrent.CreateCallback()));
  }

  std::move(concurrent)
      .Done(
          base::BindOnce(&GetIsolatedWebAppBrowsingDataCommand::CompleteCommand,
                         weak_factory_.GetWeakPtr()));

  for (auto& get_isolated_web_app_size_job : get_isolated_web_app_size_jobs_) {
    get_isolated_web_app_size_job->Start(lock_.get());
  }
}

void GetIsolatedWebAppBrowsingDataCommand::CompleteCommand(
    std::vector<std::optional<ComputedAppSizeWithOrigin>> app_size_results) {
  base::flat_map<url::Origin, uint64_t> results;
  for (const std::optional<ComputedAppSizeWithOrigin>& app_size_result :
       app_size_results) {
    if (!app_size_result) {
      continue;
    }
    CHECK(app_size_result.value().origin().has_value());
    results[app_size_result.value().origin().value()] =
        app_size_result->ComputedAppSizeWithOrigin::data_size_in_bytes();
  }
  CompleteAndSelfDestruct(CommandResult::kSuccess, std::move(results));
}

}  // namespace web_app
