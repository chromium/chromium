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
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/drive/file_errors.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {
namespace {

using ThrottleInterval = ZeroStateDriveProvider::ThrottleInterval;

constexpr char kSchema[] = "zero_state_drive://";

// How long to wait before making the first request for results from the
// ItemSuggestCache.
constexpr base::TimeDelta kFirstUpdateDelay = base::Seconds(10);

// The minimum number of results required to keep using the short delay. This
// means that results are refreshed more often if there are enough high-quality
// results returned.
constexpr size_t kShortDelayQuota = 3u;

// Outcome of a call to DriverZeroStateProvider::StartZeroState. These values
// persist to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class Status {
  kOk = 0,
  kDriveFSNotMounted = 1,
  kNoResults = 2,
  kPathLocationFailed = 3,
  kAllFilesErrored = 4,
  kDriveDisabled = 5,
  kMaxValue = kDriveDisabled,
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

void LogLatency(base::TimeDelta latency) {
  base::UmaHistogramTimes("Apps.AppList.DriveZeroStateProvider.Latency",
                          latency);
}

void SetUseLongDelay(Profile* profile, bool use_long_delay) {
  profile->GetPrefs()->SetBoolean(
      chromeos::prefs::kLauncherUseLongContinueDelay, use_long_delay);
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

bool IsDriveDisabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
}

// Filters out files that exceed the max last modified time. Files that are
// filtered out are replaced by absl::nullopt, because the length and order of
// the return vector must remain consistent with |cache_results_|.
std::vector<absl::optional<base::FilePath>> FilterPathsByTime(
    std::vector<absl::optional<base::FilePath>> paths,
    const base::TimeDelta& max_last_modified_time) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<absl::optional<base::FilePath>> filtered_paths;
  const base::Time now = base::Time::Now();
  for (const auto& path : paths) {
    base::File::Info info;
    if (path && base::PathExists(path.value()) &&
        base::GetFileInfo(path.value(), &info) &&
        (now - info.last_modified <= max_last_modified_time)) {
      filtered_paths.push_back(path);
    } else {
      filtered_paths.push_back(absl::nullopt);
    }
  }

  return filtered_paths;
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
      construction_time_(base::Time::Now()),
      item_suggest_cache_(
          profile,
          std::move(url_loader_factory),
          base::BindRepeating(&ZeroStateDriveProvider::OnCacheUpdated,
                              base::Unretained(this))),
      max_last_modified_time_(base::Days(base::GetFieldTrialParamByFeatureAsInt(
          ash::features::kProductivityLauncher,
          "max_last_modified_time",
          8))),
      enabled_(ash::features::IsProductivityLauncherEnabled()) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
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

  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher,
      "itemsuggest_query_on_filesystem_mounted", true);

  if (kUpdateCache && enabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ZeroStateDriveProvider::MaybeUpdateCache,
                       weak_factory_.GetWeakPtr()),
        kFirstUpdateDelay);
  }
}

void ZeroStateDriveProvider::OnSessionStateChanged() {
  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher,
      "itemsuggest_query_on_session_state_changed", true);

  // Perform a hypothetical query if the user has logged in.
  if (session_manager_->session_state() ==
      session_manager::SessionState::ACTIVE) {
    MaybeLogHypotheticalQuery();
    if (kUpdateCache)
      MaybeUpdateCache();
  }
}

void ZeroStateDriveProvider::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher,
      "itemsuggest_query_on_screen_idle_state_changed", true);

  // Perform a hypothetical query if the screen changed from off to on.
  if (screen_off_ && !proto.dimmed() && !proto.off()) {
    MaybeLogHypotheticalQuery();
    if (kUpdateCache)
      MaybeUpdateCache();
  }
  screen_off_ = proto.off();
}

void ZeroStateDriveProvider::ViewClosing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeLogHypotheticalQuery();

  static const bool kUpdateCache = base::GetFieldTrialParamByFeatureAsBool(
      ash::features::kProductivityLauncher, "itemsuggest_query_on_view_closing",
      true);
  if (kUpdateCache) {
    MaybeUpdateCache();
  }
}

ash::AppListSearchResultType ZeroStateDriveProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateDrive;
}

bool ZeroStateDriveProvider::ShouldBlockZeroState() const {
  return true;
}

void ZeroStateDriveProvider::Start(const std::u16string& query) {
  ClearResultsSilently();
}

void ZeroStateDriveProvider::StartZeroState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ClearResultsSilently();

  if (!enabled_)
    return;

  // Exit if drive fs isn't mounted, as we launch results via drive fs.
  if (!drive_service_ || !drive_service_->IsMounted()) {
    LogStatus(Status::kDriveFSNotMounted);
    return;
  } else if (IsDriveDisabled(profile_)) {
    LogStatus(Status::kDriveDisabled);
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
  } else if (cache_results_->results.empty()) {
    LogStatus(Status::kNoResults);
    // An empty but non-null value indicates that the cache was updated
    // successfully, and no results were returned.
    SetUseLongDelay(profile_, true);
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

  bool all_files_errored = true;
  std::vector<absl::optional<base::FilePath>> filepaths;
  for (const auto& path_or_error : paths.value()) {
    if (path_or_error->is_path()) {
      all_files_errored = false;
      filepaths.push_back(
          ReparentToDriveMount(path_or_error->get_path(), drive_service_));
    } else {
      // The length and order of |filepaths| must remain consistent with
      // |cache_results_|.
      filepaths.push_back(absl::nullopt);
    }
  }

  // We expect some files to error sometimes, but we're mainly interested in
  // when all of the files error at once. This also keeps the bucket proportion
  // of the status metric meaningful.
  if (all_files_errored) {
    LogStatus(Status::kAllFilesErrored);
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FilterPathsByTime, filepaths, max_last_modified_time_),
      base::BindOnce(&ZeroStateDriveProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateDriveProvider::SetSearchResults(
    std::vector<absl::optional<base::FilePath>> paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cache_results_);
  DCHECK_EQ(cache_results_->results.size(), paths.size());
  const auto& cache_results = cache_results_->results;

  // Assign scores to results by simply using their position in the results
  // list. The order of results from the ItemSuggest API is significant:
  // the first is better than the second, etc. Resulting scores are in [0, 1].
  const double total_items = static_cast<double>(paths.size());
  int item_index = 0;
  SearchProvider::Results provider_results;
  for (int i = 0; i < static_cast<int>(paths.size()); ++i) {
    const auto& path = paths[i];
    if (!path)
      continue;

    double score = 1.0 - (item_index / total_items);
    ++item_index;

    provider_results.emplace_back(MakeListResult(
        path.value(), cache_results[i].prediction_reason, score));
  }

  cache_results_.reset();

  // If there aren't enough results, use a long delay and vice versa. Note that
  // the delay is only updated if cache results are non-null, indicating that
  // the cache has been updated.
  SetUseLongDelay(profile_, provider_results.size() < kShortDelayQuota);

  SwapResults(&provider_results);

  LogStatus(Status::kOk);
  LogLatency(base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<FileResult> ZeroStateDriveProvider::MakeListResult(
    const base::FilePath& filepath,
    const absl::optional<std::string>& prediction_reason,
    const float relevance) {
  absl::optional<std::u16string> details;
  if (prediction_reason && ash::features::IsProductivityLauncherEnabled())
    details = base::UTF8ToUTF16(prediction_reason.value());

  auto result = std::make_unique<FileResult>(
      kSchema, filepath, details, ash::AppListSearchResultType::kZeroStateDrive,
      GetDisplayType(), relevance, std::u16string(), FileResult::Type::kFile,
      profile_);
  return result;
}

void ZeroStateDriveProvider::OnCacheUpdated() {
  StartZeroState();
}

void ZeroStateDriveProvider::MaybeUpdateCache() {
  if (!enabled_)
    return;

  if (base::Time::Now() - kFirstUpdateDelay > construction_time_) {
    item_suggest_cache_.UpdateCache();
  }
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
