// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/zero_state_file_provider.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"
#include "chrome/browser/ui/app_list/search/zero_state_file_result.h"

using file_manager::file_tasks::FileTasksObserver;

namespace app_list {
namespace {

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

}  // namespace

ZeroStateFileProvider::ZeroStateFileProvider(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // TODO(crbug.com/959679): Add a metric for if this succeeds or fails.
  if (auto* notifier =
          file_manager::file_tasks::FileTasksNotifier::GetForProfile(
              profile_)) {
    file_tasks_observer_.Add(notifier);

    RecurrenceRankerConfigProto config;
    config.set_min_seconds_between_saves(300u);
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
}

ZeroStateFileProvider::~ZeroStateFileProvider() = default;

void ZeroStateFileProvider::Start(const base::string16& query) {
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
    new_results.emplace_back(std::make_unique<ZeroStateFileResult>(
        filepath_score.first, filepath_score.second, profile_));
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
