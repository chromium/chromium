// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/zero_state_file_provider.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/files/file_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "components/prefs/pref_service.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

constexpr char kFileChipSchema[] = "file_chip://";
constexpr char kZeroStateFileSchema[] = "zero_state_file://";

constexpr int kMaxLocalFiles = 10;

// Given the output of RecurrenceRanker::RankTopN, partition files by whether
// they exist or not on disk. Returns a pair of vectors: <valid, invalid>.
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

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  auto* notifier =
      file_manager::file_tasks::FileTasksNotifier::GetForProfile(profile_);

  if (notifier) {
    file_tasks_observer_.Observe(notifier);

    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(120u);
    config.set_condition_limit(1u);
    config.set_condition_decay(0.5f);
    config.set_target_limit(200);
    config.set_target_decay(0.9f);
    config.mutable_predictor()->mutable_default_predictor();
    files_ranker_ = std::make_unique<RecurrenceRanker>(
        "ZeroStateLocalFiles",
        profile->GetPath().AppendASCII("zero_state_local_files.pb"), config,
        chromeos::ProfileHelper::IsEphemeralUserProfile(profile));
  }

  if (base::FeatureList::IsEnabled(
          app_list_features::kEnableLauncherSearchNormalization)) {
    normalizer_.emplace("zero_state_file_provider", profile, 25);
  }
}

ZeroStateFileProvider::~ZeroStateFileProvider() = default;

ash::AppListSearchResultType ZeroStateFileProvider::ResultType() {
  return ash::AppListSearchResultType::kZeroStateFile;
}

void ZeroStateFileProvider::Start(const std::u16string& query) {
  query_start_time_ = base::TimeTicks::Now();
  ClearResultsSilently();
  if (!files_ranker_ || !query.empty())
    return;

  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&ValidateFiles, files_ranker_->RankTopN(kMaxLocalFiles)),
      base::BindOnce(&ZeroStateFileProvider::SetSearchResults,
                     weak_factory_.GetWeakPtr()));
}

void ZeroStateFileProvider::SetSearchResults(
    const internal::ValidAndInvalidResults& results) {
  // Delete invalid results from the model.
  for (const auto& path : results.second)
    files_ranker_->RemoveTarget(path.value());

  // Use valid results for search results.
  SearchProvider::Results new_results;
  for (const auto& filepath_score : results.first) {
    new_results.emplace_back(std::make_unique<FileResult>(
        kZeroStateFileSchema, filepath_score.first,
        ash::AppListSearchResultType::kZeroStateFile,
        ash::SearchResultDisplayType::kList, filepath_score.second, profile_));

    // Add suggestion chip file results
    if (app_list_features::IsSuggestedFilesEnabled() &&
        IsSuggestedContentEnabled(profile_)) {
      new_results.emplace_back(
          std::make_unique<FileResult>(kFileChipSchema, filepath_score.first,
                                       ash::AppListSearchResultType::kFileChip,
                                       ash::SearchResultDisplayType::kChip,
                                       filepath_score.second, profile_));
    }
  }

  if (normalizer_.has_value()) {
    normalizer_->RecordResults(new_results);
    normalizer_->NormalizeResults(&new_results);
  }

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
      files_ranker_->Record(file_open.path.value());
  }
}

}  // namespace app_list
