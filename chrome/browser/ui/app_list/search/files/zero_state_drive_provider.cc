// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_drive_provider.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace app_list {
namespace {

using ThrottleInterval = ZeroStateDriveProvider::ThrottleInterval;

// Schemas of result IDs for the results list and suggestion chips.
// TODO(crbug.com/1258415): kChipSchema can be removed once the new launcher is
// launched.
constexpr char kListSchema[] = "zero_state_drive://";
constexpr char kChipSchema[] = "drive_chip://";

// Outcome of a call to DriverZeroStateProvider::StartZeroState. These values
// persist to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class Status {
  kOk = 0,
  kDriveFSNotMounted = 1,
  kNoResults = 2,
  kPathLocationFailed = 3,
  kAllFilesErrored = 4,
  kMaxValue = kAllFilesErrored,
};

ThrottleInterval MinutesToThrottleInterval(const int minutes) {
  switch (minutes) {
    case 5:
      return ThrottleInterval::kFiveMinutes;
    case 10:
      return ThrottleInterval::kTenMinutes;
    case 15:
      return ThrottleInterval::kFifteenMinutes;
    case 30:
      return ThrottleInterval::kThirtyMinutes;
    default:
      return ThrottleInterval::kUnknown;
  }
}

void LogStatus(Status status) {
  base::UmaHistogramEnumeration("Apps.AppList.DriveZeroStateProvider.Status",
                                status);
}

void LogShouldWarm(bool should_warm) {
  base::UmaHistogramBoolean("Apps.AppList.DriveZeroStateProvider.ShouldWarm",
                            should_warm);
}

void LogLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Apps.AppList.DriveZeroStateProvider.Latency",
                          latency);
}

bool IsSuggestedContentEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      chromeos::prefs::kSuggestedContentEnabled);
}

// Given an absolute path representing a file in the user's Drive, returns a
// reparented version of the path within the user's drive fs mount.
base::FilePath ReparentToDriveMount(
    const base::FilePath& path,
    const drive::DriveIntegrationService* drive_service) {
  DCHECK(!path.IsAbsolute());
  return drive_service->GetMountPointPath().Append(path.value());
}

// TODO(crbug.com/1258415): This exists to reroute results depending on which
// launcher is enabled, and should be removed after the new launcher launch.
ash::SearchResultDisplayType GetDisplayType() {
  return ash::features::IsProductivityLauncherEnabled()
             ? ash::SearchResultDisplayType::kContinue
             : ash::SearchResultDisplayType::kList;
}

}  // namespace

ZeroStateDriveProvider::ZeroStateDriveProvider(
    Profile* profile,
    SearchController* search_controller,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : profile_(profile),
      drive_service_(
          drive::DriveIntegrationServiceFactory::GetForProfile(profile)),
      session_manager_(session_manager::SessionManager::Get()),
      item_suggest_cache_(profile, std::move(url_loader_factory)),
      suggested_files_enabled_(app_list_features::IsSuggestedFilesEnabled()) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  if (drive_service_) {
    if (drive_service_->IsMounted()) {
      // DriveFS is mounted, so we can fetch results immediately.
      OnFileSystemMounted();
    } else {
      // Wait for DriveFS to be mounted, then fetch results. This happens in
      // OnFileSystemMounted.
      drive_observation_.Observe(drive_service_);
    }
  }

  if (session_manager_)
    session_observation_.Observe(session_manager_);

  auto* power_manager = chromeos::PowerManagerClient::Get();
  if (power_manager)
    power_observation_.Observe(power_manager);
}

ZeroStateDriveProvider::~ZeroStateDriveProvider() = default;

void ZeroStateDriveProvider::OnFileSystemMounted() {
  MaybeLogHypotheticalQuery();

  // Warm the results cache if or when drivefs is mounted by fetching from the
  // Drive QuickAccess API. This is necessary only if the suggested files
  // experiment is enabled, so that results are ready for display in the
  // suggested chips on the first launcher open after login. To prevent
  // unnecessary queries to ItemSuggest, only warm the cache if the launcher has
  // been used before.
  const bool launcher_used = profile_->GetPrefs()->GetBoolean(
      chromeos::prefs::kLauncherResultEverLaunched);
  const bool gate_on_use = base::GetFieldTrialParamByFeatureAsBool(
      app_list_features::kEnableSuggestedFiles, "gate_warm_on_launcher_use",
      true);
  const bool should_warm = !gate_on_use || launcher_used;
  LogShouldWarm(should_warm);

  if (have_warmed_up_cache_ || !suggested_files_enabled_ || !should_warm)
    return;
  have_warmed_up_cache_ = true;
  item_suggest_cache_.UpdateCache();
}

void ZeroStateDriveProvider::OnSessionStateChanged() {
  // Perform a hypothetical query if the user has logged in.
  if (session_manager_->session_state() ==
      session_manager::SessionState::ACTIVE) {
    MaybeLogHypotheticalQuery();
  }
}

void ZeroStateDriveProvider::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  // Perform a hypothetical query if the screen changed from off to on.
  if (screen_off_ && !proto.dimmed() && !proto.off()) {
    MaybeLogHypotheticalQuery();
  }
  screen_off_ = proto.off();
}

void ZeroStateDriveProvider::AppListShown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeLogHypotheticalQuery();
  item_suggest_cache_.UpdateCache();
}

ash::AppListSearchResultType ZeroStateDriveProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateDrive;
}

bool ZeroStateDriveProvider::ShouldBlockZeroState() const {
  return true;
}

void ZeroStateDriveProvider::StartZeroState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();

  // TODO(crbug.com/1034842): Add query latency metrics.

  // Exit if drive fs isn't mounted, as we launch results via drive fs.
  if (!drive_service_ || !drive_service_->IsMounted()) {
    LogStatus(Status::kDriveFSNotMounted);
    return;
  }

  query_start_time_ = base::TimeTicks::Now();

  // Cancel any in-flight queries for this provider.
  weak_factory_.InvalidateWeakPtrs();

  // Get the most recent results from the cache.
  cache_results_ = item_suggest_cache_.GetResults();
  if (!cache_results_) {
    LogStatus(Status::kNoResults);
    return;
  }

  std::vector<std::string> item_ids;
  for (const auto& result : cache_results_->results) {
    item_ids.push_back(result.id);
  }

  drive_service_->LocateFilesByItemIds(
      item_ids, base::BindOnce(&ZeroStateDriveProvider::OnFilePathsLocated,
                               weak_factory_.GetWeakPtr()));
}

void ZeroStateDriveProvider::OnFilePathsLocated(
    absl::optional<std::vector<drivefs::mojom::FilePathOrErrorPtr>> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!paths) {
    LogStatus(Status::kPathLocationFailed);
    return;
  }

  DCHECK(cache_results_);
  DCHECK_EQ(cache_results_->results.size(), paths->size());

  // Assign scores to results by simply using their position in the results
  // list. The order of results from the ItemSuggest API is significant:
  // the first is better than the second, etc. Resulting scores are in [0, 1].
  const double total_items = static_cast<double>(paths->size());
  int item_index = 0;
  bool all_files_errored = true;
  SearchProvider::Results provider_results;
  for (int i = 0; i < static_cast<int>(paths->size()); ++i) {
    const auto& path_or_error = paths.value()[i];
    if (path_or_error->is_error()) {
      continue;
    } else {
      all_files_errored = false;
    }

    double score = 1.0 - (item_index / total_items);
    ++item_index;

    // TODO(crbug.com/1034842): Use |cache_results_| to attach the session id to
    // the result.

    provider_results.emplace_back(
        MakeListResult(path_or_error->get_path(), score));
    if (suggested_files_enabled_ && IsSuggestedContentEnabled(profile_)) {
      provider_results.emplace_back(
          MakeChipResult(path_or_error->get_path(), score));
    }
  }

  // We expect some files to error sometimes, but we're mainly interested in
  // when all of the files error at once. This also keeps the bucket proportion
  // of the status metric meaningful.
  if (all_files_errored) {
    LogStatus(Status::kAllFilesErrored);
    return;
  }

  cache_results_.reset();

  SwapResults(&provider_results);

  LogStatus(Status::kOk);
  LogLatency(base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> ZeroStateDriveProvider::MakeListResult(
    const base::FilePath& filepath,
    const float relevance) {
  return std::make_unique<FileResult>(
      kListSchema, ReparentToDriveMount(filepath, drive_service_),
      ash::AppListSearchResultType::kZeroStateDrive, GetDisplayType(),
      relevance, std::u16string(), FileResult::Type::kFile, profile_);
}

std::unique_ptr<FileResult> ZeroStateDriveProvider::MakeChipResult(
    const base::FilePath& filepath,
    const float relevance) {
  return std::make_unique<FileResult>(
      kChipSchema, ReparentToDriveMount(filepath, drive_service_),
      ash::AppListSearchResultType::kDriveChip,
      ash::SearchResultDisplayType::kChip, relevance, std::u16string(),
      FileResult::Type::kFile, profile_);
}

void ZeroStateDriveProvider::MaybeLogHypotheticalQuery() {
  const auto now = base::TimeTicks::Now();
  const std::vector<int> throttle_intervals({5, 10, 15, 30});

  for (const int interval : throttle_intervals) {
    const bool is_first_query = last_hypothetical_query_.find(interval) ==
                                last_hypothetical_query_.end();
    if (is_first_query ||
        now - last_hypothetical_query_[interval] >= base::Minutes(interval)) {
      base::UmaHistogramEnumeration(
          "Apps.AppList.DriveZeroStateProvider.HypotheticalQuery",
          MinutesToThrottleInterval(interval));
      last_hypothetical_query_[interval] = now;
    }
  }
}

}  // namespace app_list
