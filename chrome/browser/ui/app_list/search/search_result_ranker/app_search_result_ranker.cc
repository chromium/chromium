// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.pb.h"

namespace app_list {
namespace {

constexpr char kAppLaunchPredictorFilename[] = "app_launch_predictor";

// Returns a AppLaunchPredictor pointer based on the |predictor_name|.
std::unique_ptr<AppLaunchPredictor> CreatePredictor(
    const std::string& predictor_name) {
  if (predictor_name == MrfuAppLaunchPredictor::kPredictorName)
    return std::make_unique<MrfuAppLaunchPredictor>();

  if (predictor_name == SerializedMrfuAppLaunchPredictor::kPredictorName)
    return std::make_unique<SerializedMrfuAppLaunchPredictor>();

  if (predictor_name == HourAppLaunchPredictor::kPredictorName)
    return std::make_unique<HourAppLaunchPredictor>();

  if (predictor_name == FakeAppLaunchPredictor::kPredictorName)
    return std::make_unique<FakeAppLaunchPredictor>();

  NOTREACHED();
  return nullptr;
}

// Save |proto| to |predictor_filename|.
void SaveToDiskOnWorkerThread(const base::FilePath& predictor_filename,
                              const AppLaunchPredictorProto& proto) {

  std::string proto_str;
  if (!proto.SerializeToString(&proto_str)) {
    LOG(ERROR)
        << "Unable to serialize AppLaunchPredictorProto, not saving to disk.";
    return;
  }
  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        predictor_filename, proto_str, "AppSearchResultRanker");
  }
  if (!write_result) {
    LOG(ERROR) << "Error writing predictor file " << predictor_filename;
  }
}

// Loads a AppLaunchPredictor from |predictor_filename|.
std::unique_ptr<AppLaunchPredictor> LoadPredictorFromDiskOnWorkerThread(
    const base::FilePath& predictor_filename,
    const std::string predictor_name) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Loads proto string from local disk.
  std::string proto_str;
  if (!base::ReadFileToString(predictor_filename, &proto_str))
    return nullptr;

  // Parses proto string as AppLaunchPredictorProto.
  AppLaunchPredictorProto proto;
  if (!proto.ParseFromString(proto_str))
    return nullptr;

  auto predictor = CreatePredictor(predictor_name);
  // Initializes the |predictor_| from the |proto|.
  if (!predictor->FromProto(proto))
    return nullptr;

  return predictor;
}

}  // namespace

AppSearchResultRanker::AppSearchResultRanker(const base::FilePath& profile_path,
                                             bool is_ephemeral_user)
    : predictor_filename_(
          profile_path.AppendASCII(kAppLaunchPredictorFilename)) {
  if (!app_list_features::IsZeroStateAppsRankerEnabled()) {
    LOG(ERROR) << "AppSearchResultRanker: ZeroStateAppsRanker is not enabled.";
    return;
  }
  // TODO(charleszhao): remove these logs once the test review is done.
  LOG(ERROR) << "AppSearchResultRanker::AppSearchResultRankerPredictorName "
             << app_list_features::AppSearchResultRankerPredictorName();
  predictor_ =
      CreatePredictor(app_list_features::AppSearchResultRankerPredictorName());

  // MrfuAppLaunchPredictor doesn't have materialization, so no loading from
  // local disk.
  if (predictor_->GetPredictorName() ==
      MrfuAppLaunchPredictor::kPredictorName) {
    load_from_disk_completed_ = true;
    return;
  }

  // For ephemeral users, we disable AppSearchResultRanker to make finch
  // experiment easier.
  if (is_ephemeral_user)
    return;

  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Loads the predictor from disk asynchronously.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadPredictorFromDiskOnWorkerThread, predictor_filename_,
                     predictor_->GetPredictorName()),
      base::BindOnce(&AppSearchResultRanker::OnLoadFromDiskComplete,
                     weak_factory_.GetWeakPtr()));
}

AppSearchResultRanker::~AppSearchResultRanker() = default;

void AppSearchResultRanker::Train(const std::string& app_id) {
  if (load_from_disk_completed_) {
    predictor_->Train(app_id);

    if (predictor_->ShouldSave()) {
      // Writes the predictor proto to disk asynchronously.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SaveToDiskOnWorkerThread, predictor_filename_,
                         predictor_->ToProto()));
    }
  }
}

base::flat_map<std::string, float> AppSearchResultRanker::Rank() {
  if (load_from_disk_completed_) {
    return predictor_->Rank();
  }

  return {};
}

void AppSearchResultRanker::OnLoadFromDiskComplete(
    std::unique_ptr<AppLaunchPredictor> predictor) {
  if (predictor) {
    predictor_.swap(predictor);
  }
  load_from_disk_completed_ = true;
  LOG(ERROR) << "AppSearchResultRanker::OnLoadFromDiskComplete "
             << predictor_->GetPredictorName();
}

}  // namespace app_list
