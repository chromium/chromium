// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/files/justifications.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/util/persistent_proto.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

constexpr char kSchema[] = "zero_state_file://";

constexpr base::TimeDelta kSaveDelay = base::Seconds(3);

constexpr size_t kMaxLocalFiles = 10u;

// Given the output of MrfuCache::GetAll, partition files into:
// - valid files that exist on-disk and have been modified in the last
//   |max_last_modified_time| days
// - invalid files, otherwise.
internal::ValidAndInvalidResults ValidateFiles(
    const std::vector<std::pair<std::string, float>>& ranker_results,
    const base::TimeDelta& max_last_modified_time) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  internal::ScoredResults valid;
  internal::Results invalid;
  const base::Time now = base::Time::Now();
  for (const auto& path_score : ranker_results) {
    // We use FilePath::FromUTF8Unsafe to decode the filepath string. As per its
    // documentation, this is a safe use of the function because
    // ZeroStateFileProvider is only used on ChromeOS, for which
    // filepaths are UTF8.
    const auto& path = base::FilePath::FromUTF8Unsafe(path_score.first);

    base::File::Info info;
    if (base::PathExists(path) && base::GetFileInfo(path, &info) &&
        (now - info.last_modified <= max_last_modified_time)) {
      valid.emplace_back(path, path_score.second);
    } else {
      invalid.emplace_back(path);
    }
  }
  return {valid, invalid};
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

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : profile_(profile),
      thumbnail_loader_(profile),
      max_last_modified_time_(base::Days(base::GetFieldTrialParamByFeatureAsInt(
          ash::features::kProductivityLauncher,
          "max_last_modified_time",
          8))) {
  DCHECK(profile_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  auto* notifier =
      file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile_);

  if (notifier) {
    file_tasks_observer_.Observe(notifier);

    MrfuCache::Params params;
    // 5 consecutive clicks to get a new file to a score of 0.8, and 10 clicks
    // on other files to reduce its score by half.
    params.half_life = 10.0f;
    params.boost_factor = 5.0f;
    MrfuCache::Proto proto(
        RankerStateDirectory(profile).AppendASCII("zero_state_local_files.pb"),
        kSaveDelay);
    proto.RegisterOnRead(base::BindOnce(
        &ZeroStateFileProvider::OnProtoInitialized, base::Unretained(this)));
    files_ranker_ = std::make_unique<MrfuCache>(std::move(proto), params);
  }
}

void ZeroStateFileProvider::OnProtoInitialized(ReadStatus status) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ValidateFiles, files_ranker_->GetAll(),
                     max_last_modified_time_),
      base::BindOnce(&ZeroStateFileProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

ZeroStateFileProvider::~ZeroStateFileProvider() = default;

ash::AppListSearchResultType ZeroStateFileProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateFile;
}

bool ZeroStateFileProvider::ShouldBlockZeroState() const {
  return true;
}

void ZeroStateFileProvider::Start(const std::u16string& query) {
  ClearResultsSilently();
}

void ZeroStateFileProvider::StartZeroState() {
  query_start_time_ = base::TimeTicks::Now();
  ClearResultsSilently();

  // Despite this being for zero-state _local_ files only, we disable all
  // results in the Continue section if Drive is disabled.
  if (!files_ranker_ || IsDriveDisabled(profile_)) {
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ValidateFiles, files_ranker_->GetAll(),
                     max_last_modified_time_),
      base::BindOnce(&ZeroStateFileProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateFileProvider::SetSearchResults(
    internal::ValidAndInvalidResults results) {
  // Delete invalid results from the ranker.
  for (const base::FilePath& path : results.second)
    files_ranker_->Delete(path.value());

  // Sort valid results high-to-low by score.
  auto& valid_results = results.first;
  std::sort(valid_results.begin(), valid_results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Use valid results for search results.
  SearchProvider::Results new_results;
  for (size_t i = 0; i < std::min(valid_results.size(), kMaxLocalFiles); ++i) {
    const auto& filepath = valid_results[i].first;
    double score = valid_results[i].second;
    auto result = std::make_unique<FileResult>(
        kSchema, filepath, absl::nullopt,
        ash::AppListSearchResultType::kZeroStateFile, GetDisplayType(), score,
        std::u16string(), FileResult::Type::kFile, profile_);
    result->SetDetailsToJustificationString();
    // TODO(crbug.com/1258415): Only generate thumbnails if the old launcher is
    // enabled. We should implement new thumbnail logic for Continue results if
    // necessary.
    if (result->display_type() == ash::SearchResultDisplayType::kList) {
      result->RequestThumbnail(&thumbnail_loader_);
    }
    new_results.push_back(std::move(result));
  }

  if (app_list_features::IsForceShowContinueSectionEnabled())
    AppendFakeSearchResults(&new_results);

  UMA_HISTOGRAM_TIMES("Apps.AppList.ZeroStateFileProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
  SwapResults(&new_results);
}

void ZeroStateFileProvider::OnFilesOpened(
    const std::vector<FileOpenEvent>& file_opens) {
  if (!files_ranker_)
    return;

  const auto& profile_path = profile_->GetPath();
  for (const auto& file_open : file_opens) {
    // Filter out file opens if:
    // 1. The open event is not a kLaunch or a kOpen.
    if (file_open.open_type != FileTasksObserver::OpenType::kLaunch &&
        file_open.open_type != FileTasksObserver::OpenType::kOpen) {
      continue;
    }

    // 2. The open relates to a Drive file, which is handled by another
    // provider. Filter this out by checking if the file resides in the user's
    // cryptohome.
    if (!profile_path.AppendRelativePath(file_open.path, nullptr)) {
      continue;
    }

    files_ranker_->Use(file_open.path.value());
  }
}

void ZeroStateFileProvider::AppendFakeSearchResults(Results* results) {
  constexpr int kTotalFakeFiles = 3;
  for (int i = 0; i < kTotalFakeFiles; ++i) {
    results->emplace_back(std::make_unique<FileResult>(
        kSchema,
        base::FilePath(FILE_PATH_LITERAL(
            base::StrCat({"Fake-file-", base::NumberToString(i), ".png"}))),
        u"-", ash::AppListSearchResultType::kZeroStateFile,
        ash::SearchResultDisplayType::kContinue, 0.1f, std::u16string(),
        FileResult::Type::kFile, profile_));
  }
}

}  // namespace app_list
