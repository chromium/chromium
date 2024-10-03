// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/dedupe_install_urls_command.h"

#include "base/barrier_closure.h"
#include "base/containers/extend.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

using ExternalManagementConfig = WebApp::ExternalManagementConfig;
using ExternalConfigMap = WebApp::ExternalConfigMap;

namespace {

bool g_suppress_for_testing = false;

base::flat_map<GURL, base::flat_set<webapps::AppId>> BuildInstallUrlToAppIdsMap(
    const WebAppRegistrar& registrar,
    base::Value::Dict& debug_value) {
  base::flat_map<GURL, base::flat_set<webapps::AppId>> result;

  for (const WebApp& app : registrar.GetApps()) {
    for (const auto& [install_source, config] :
         app.management_to_external_config_map()) {
      for (const GURL& install_url : config.install_urls) {
        result[install_url].insert(app.app_id());
        debug_value.EnsureList(install_url.spec())->Append(app.app_id());
      }
    }
  }

  return result;
}

const webapps::AppId& SelectWebAppToDedupeInto(
    const WebAppRegistrar& registrar,
    const base::flat_set<webapps::AppId>& app_ids_with_common_install_url) {
  CHECK(app_ids_with_common_install_url.size() > 1);

  const webapps::AppId* best = nullptr;
  bool best_looks_like_placeholder = false;
  base::Time best_install_time;

  for (const webapps::AppId& app_id : app_ids_with_common_install_url) {
    const WebApp& candidate = *registrar.GetAppById(app_id);
    bool candidate_looks_like_placeholder = LooksLikePlaceholder(candidate);

    if (
        // Is this the first candidate we've seen?
        !best ||
        // Is the candidate an upgrade from a placeholder?
        (best_looks_like_placeholder && !candidate_looks_like_placeholder) ||
        // Is the candidate more recently installed and not a downgrade?
        (best_looks_like_placeholder == candidate_looks_like_placeholder &&
         candidate.latest_install_time() > best_install_time)) {
      best = &app_id;
      best_looks_like_placeholder = candidate_looks_like_placeholder;
      best_install_time = candidate.latest_install_time();
    }
  }

  CHECK(best);
  return *best;
}

std::vector<std::unique_ptr<RemoveInstallUrlJob>>
BuildOperationsToDedupeInstallUrlConfigsIntoSelectedApp(
    Profile& profile,
    base::Value::Dict& jobs_debug_value,
    const WebAppRegistrar& registrar,
    ScopedRegistryUpdate& update,
    const GURL& install_url,
    const base::flat_set<webapps::AppId>& app_ids_with_common_install_url,
    const webapps::AppId& id_to_dedupe_into) {
  std::vector<std::unique_ptr<RemoveInstallUrlJob>> result;

  WebApp& app_to_dedupe_into = *update->UpdateApp(id_to_dedupe_into);

  for (const webapps::AppId& id_to_dedupe_out_of :
       app_ids_with_common_install_url) {
    if (id_to_dedupe_out_of == id_to_dedupe_into) {
      continue;
    }

    const WebApp& app_to_dedupe_out_of =
        *registrar.GetAppById(id_to_dedupe_out_of);

    for (auto const& [install_source, config_to_dedupe_out_of] :
         app_to_dedupe_out_of.management_to_external_config_map()) {
      if (!config_to_dedupe_out_of.install_urls.contains(install_url)) {
        continue;
      }

      app_to_dedupe_into.AddSource(install_source);
      app_to_dedupe_into.AddInstallURLToManagementExternalConfigMap(
          install_source, install_url);
      for (const std::string& policy_id :
           config_to_dedupe_out_of.additional_policy_ids) {
        app_to_dedupe_into.AddPolicyIdToManagementExternalConfigMap(
            install_source, policy_id);
      }

      // Create job to remove deduped install URL from existing app.
      result.push_back(std::make_unique<RemoveInstallUrlJob>(
          webapps::WebappUninstallSource::kInstallUrlDeduping, profile,
          *jobs_debug_value.EnsureDict(id_to_dedupe_out_of),
          id_to_dedupe_out_of, install_source, install_url));
    }
  }

  return result;
}

struct DedupeOperations {
  std::vector<std::unique_ptr<RemoveInstallUrlJob>> remove_install_url_jobs;
  base::flat_map<GURL, webapps::AppId> dedupe_choices;
};

DedupeOperations BuildOperationsToHaveOneAppPerInstallUrl(
    Profile& profile,
    base::Value::Dict& debug_value,
    const WebAppRegistrar& registrar,
    ScopedRegistryUpdate& update,
    const base::flat_map<GURL, base::flat_set<webapps::AppId>>&
        install_url_to_apps) {
  DedupeOperations result;

  for (const auto& [install_url, app_ids] : install_url_to_apps) {
    if (app_ids.size() <= 1) {
      continue;
    }

    const webapps::AppId& id_to_dedupe_into =
        SelectWebAppToDedupeInto(registrar, app_ids);
    result.dedupe_choices[install_url] = id_to_dedupe_into;
    debug_value.EnsureDict("dedupe_choices")
        ->Set(install_url.spec(), id_to_dedupe_into);

    base::Extend(
        result.remove_install_url_jobs,
        BuildOperationsToDedupeInstallUrlConfigsIntoSelectedApp(
            profile, *debug_value.EnsureDict("removal_jobs"), registrar, update,
            install_url, app_ids, id_to_dedupe_into));
  }

  return result;
}

}  // namespace

base::AutoReset<bool> DedupeInstallUrlsCommand::ScopedSuppressForTesting() {
  return {&g_suppress_for_testing, true};
}

DedupeInstallUrlsCommand::DedupeInstallUrlsCommand(
    Profile& profile,
    base::OnceClosure completed_callback)
    : WebAppCommand("DedupeInstallUrlsCommand",
                    AllAppsLockDescription(),
                    std::move(completed_callback)),
      profile_(profile) {}

DedupeInstallUrlsCommand::~DedupeInstallUrlsCommand() = default;

void DedupeInstallUrlsCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  if (g_suppress_for_testing) {
    CompleteAndSelfDestruct(CommandResult::kSuccess);
    return;
  }

  lock_ = std::move(lock);

  install_url_to_apps_ = BuildInstallUrlToAppIdsMap(
      lock_->registrar(),
      *GetMutableDebugValue().EnsureDict("duplicate_install_urls"));

  {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    DedupeOperations pending_dedupe_operations =
        BuildOperationsToHaveOneAppPerInstallUrl(
            profile_.get(), GetMutableDebugValue(), lock_->registrar(), update,
            install_url_to_apps_);

    dedupe_choices_ = std::move(pending_dedupe_operations.dedupe_choices);
    pending_jobs_ =
        std::move(pending_dedupe_operations.remove_install_url_jobs);
  }

  ProcessPendingJobsOrComplete();
}

void DedupeInstallUrlsCommand::ProcessPendingJobsOrComplete() {
  CHECK(!active_job_);

  if (!pending_jobs_.empty()) {
    std::swap(active_job_, pending_jobs_.back());
    pending_jobs_.pop_back();
    active_job_->Start(*lock_,
                       base::BindOnce(&DedupeInstallUrlsCommand::JobComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  RecordMetrics();
  CompleteAndSelfDestruct(any_errors_ ? CommandResult::kFailure
                                      : CommandResult::kSuccess);
}

void DedupeInstallUrlsCommand::JobComplete(webapps::UninstallResultCode code) {
  CHECK(active_job_);

  if (!UninstallSucceeded(code)) {
    any_errors_ = true;
  }

  active_job_.reset();

  ProcessPendingJobsOrComplete();
}

void DedupeInstallUrlsCommand::RecordMetrics() {
  size_t dedupe_count = 0;
  for (const auto& [install_url, app_ids] : install_url_to_apps_) {
    if (app_ids.size() <= 1) {
      continue;
    }
    ++dedupe_count;
    base::UmaHistogramCounts100("WebApp.DedupeInstallUrls.AppsDeduped",
                                app_ids.size());
  }
  base::UmaHistogramCounts100("WebApp.DedupeInstallUrls.InstallUrlsDeduped",
                              dedupe_count);
}

}  // namespace web_app
