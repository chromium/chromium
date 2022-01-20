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
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/ranking/util.h"
#include "chrome/browser/ui/app_list/search/util/persistent_proto.h"
#include "components/prefs/pref_service.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

// TODO(crbug.com/1258415): kFileChipSchema can be removed once the new
// launcher is launched.
constexpr char kFileChipSchema[] = "file_chip://";
constexpr char kZeroStateFileSchema[] = "zero_state_file://";

constexpr base::TimeDelta kSaveDelay = base::Seconds(3);

constexpr size_t kMaxLocalFiles = 10u;

// Given the output of MrfuCache::GetAll, partition files by whether or not they
// exist on disk. Returns a pair of vectors: <valid, invalid>.
internal::ValidAndInvalidResults ValidateFiles(
    const std::vector<std::pair<std::string, float>>& ranker_results) {
  internal::ScoredResults valid;
  internal::Results invalid;
  for (const auto& path_score : ranker_results) {
    // We use FilePath::FromUTF8Unsafe to decode the filepath string. As per its
    // documentation, this is a safe use of the function because
    // ZeroStateFileProvider is only used on ChromeOS, for which
    // filepaths are UTF8.
    const auto& path = base::FilePath::FromUTF8Unsafe(path_score.first);
    if (base::PathExists(path))
      valid.emplace_back(path, path_score.second);
    else
      invalid.emplace_back(path);
  }
  return {valid, invalid};
}

bool IsSuggestedContentEnabled(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      chromeos::prefs::kSuggestedContentEnabled);
}

// TODO(crbug.com/1258415): This exists to reroute results depending on which
// launcher is enabled, and should be removed after the new launcher launch.
ash::SearchResultDisplayType GetDisplayType() {
  return ash::features::IsProductivityLauncherEnabled()
             ? ash::SearchResultDisplayType::kContinue
             : ash::SearchResultDisplayType::kList;
}

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : profile_(profile), thumbnail_loader_(profile) {
  DCHECK(profile_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  auto* notifier =
      file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile_);

  if (notifier) {
    file_tasks_observer_.Observe(notifier);

    MrfuCache::Params params;
    // 5 consecutive clicks to get a new file to a score of 2/3, and 10 clicks
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
      base::BindOnce(&ValidateFiles, files_ranker_->GetAll()),
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

void ZeroStateFileProvider::StartZeroState() {
  query_start_time_ = base::TimeTicks::Now();
  ClearResultsSilently();
  if (!files_ranker_) {
    return;
  }

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ValidateFiles, files_ranker_->GetAll()),
      base::BindOnce(&ZeroStateFileProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateFileProvider::SetSearchResults(
    internal::ValidAndInvalidResults results) {
  // TODO(crbug.com/1258415): Re-add deletion of invalid results from the model.

  // Sort valid results high-to-low by score.
  auto& valid_results = results.first;
  std::sort(valid_results.begin(), valid_results.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Use valid results for search results.
  SearchProvider::Results new_results;
  for (size_t i = 0; i < std::min(valid_results.size(), kMaxLocalFiles); ++i) {
    const auto& filepath_score = valid_results[i];
    double score = filepath_score.second;
    auto result = std::make_unique<FileResult>(
        kZeroStateFileSchema, filepath_score.first,
        ash::AppListSearchResultType::kZeroStateFile, GetDisplayType(), score,
        std::u16string(), FileResult::Type::kFile, profile_);
    // TODO(crbug.com/1258415): Only generate thumbnails if the old launcher is
    // enabled. We should implement new thumbnail logic for Continue results if
    // necessary.
    if (result->display_type() == ash::SearchResultDisplayType::kList) {
      result->RequestThumbnail(&thumbnail_loader_);
    }
    new_results.push_back(std::move(result));

    // Add suggestion chip file results.
    // TODO(crbug.com/1258415): This can be removed once the new launcher is
    // launched.
    if (app_list_features::IsSuggestedLocalFilesEnabled() &&
        IsSuggestedContentEnabled(profile_)) {
      new_results.emplace_back(std::make_unique<FileResult>(
          kFileChipSchema, filepath_score.first,
          ash::AppListSearchResultType::kFileChip,
          ash::SearchResultDisplayType::kChip, filepath_score.second,
          std::u16string(), FileResult::Type::kFile, profile_));
    }
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

  // The DriveQuickAccessProvider handles Drive files, so recording them here
  // would be redundant. Filter them out by checking the file resides within the
  // user's cryptohome.
  const auto& profile_path = profile_->GetPath();
  for (const auto& file_open : file_opens) {
    if (profile_path.AppendRelativePath(file_open.path, nullptr))
      files_ranker_->Use(file_open.path.value());
  }
}

void ZeroStateFileProvider::AppendFakeSearchResults(Results* results) {
  constexpr int kTotalFakeFiles = 3;
  for (int i = 0; i < kTotalFakeFiles; ++i) {
    results->emplace_back(std::make_unique<FileResult>(
        kFileChipSchema,
        base::FilePath(FILE_PATH_LITERAL(
            base::StrCat({"Fake-file-", base::NumberToString(i), ".png"}))),
        ash::AppListSearchResultType::kFileChip,
        ash::SearchResultDisplayType::kContinue, 0.1f, std::u16string(),
        FileResult::Type::kFile, profile_));
  }
}

}  // namespace app_list
