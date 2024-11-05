// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/get_isolated_web_app_browsing_data_command.h"

#include <memory>
#include <optional>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
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
    : WebAppCommand<AllAppsLock, base::flat_map<url::Origin, int64_t>>(
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

  const auto isolated_web_apps = GetInstalledIwas(lock_->registrar());
  if (!isolated_web_apps.empty()) {
    auto result_callback =
        base::BarrierCallback<std::optional<GetIsolatedWebAppSizeJobResult>>(
            isolated_web_apps.size(),
            base::BindOnce(
                &GetIsolatedWebAppBrowsingDataCommand::CompleteCommand,
                weak_factory_.GetWeakPtr()));
    for (const auto& [bundle_id, isolated_web_app] : isolated_web_apps) {
      get_isolated_web_app_size_jobs_.push_back(
          std::make_unique<GetIsolatedWebAppSizeJob>(
              &profile_.get(), isolated_web_app.get().app_id(),
              GetMutableDebugValue(), result_callback));
    }

    for (auto& get_isolated_web_app_size_job :
         get_isolated_web_app_size_jobs_) {
      get_isolated_web_app_size_job->Start(lock_.get());
    }
  } else {
    CompleteCommand(/*app_size_results=*/{});
  }
}

void GetIsolatedWebAppBrowsingDataCommand::CompleteCommand(
    std::vector<std::optional<GetIsolatedWebAppSizeJobResult>>
        app_size_results) {
  base::flat_map<url::Origin, int64_t> results;
  for (const std::optional<GetIsolatedWebAppSizeJobResult>& app_size_result :
       app_size_results) {
    if (!app_size_result) {
      continue;
    }
    results[app_size_result->iwa_origin] = app_size_result->app_size;
  }
  CompleteAndSelfDestruct(CommandResult::kSuccess, std::move(results));
}

}  // namespace web_app
