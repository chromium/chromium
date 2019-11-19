// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_util.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"

namespace app_list {
namespace {

using base::Optional;
using base::Time;
using base::TimeDelta;

// A predictor may return scores for target IDs that have been deleted. If less
// than this proportion of IDs are valid, the ranker triggers a cleanup of the
// predictor's state on a call to RecurrenceRanker::Rank.
constexpr float kMinValidTargetProportionBeforeCleanup = 0.5f;

void SaveProtoToDisk(const base::FilePath& filepath,
                     const std::string& model_identifier,
                     const RecurrenceRankerProto& proto) {
  std::string proto_str;
  if (!proto.SerializeToString(&proto_str)) {
    LogSerializationStatus(model_identifier,
                           SerializationStatus::kToProtoError);
    return;
  }

  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        filepath, proto_str, "RecurrenceRanker");
  }
  if (!write_result) {
    LogSerializationStatus(model_identifier,
                           SerializationStatus::kModelWriteError);
    return;
  }

  LogSerializationStatus(model_identifier, SerializationStatus::kSaveOk);
}

// Try to load a |RecurrenceRankerProto| from the given filepath. If it fails,
// it returns nullptr.
std::unique_ptr<RecurrenceRankerProto> LoadProtoFromDisk(
    const base::FilePath& filepath,
    const std::string& model_identifier) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // TODO(crbug.com/955893): for debugging, this prevents loading models for
  // zero-state files ranking on dev channel. To be removed when no longer
  // necessary.
  if (model_identifier == "ZeroStateGroups" &&
      chrome::GetChannel() == version_info::Channel::DEV) {
    return nullptr;
  }

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str)) {
    LogSerializationStatus(model_identifier,
                           SerializationStatus::kModelReadError);
    return nullptr;
  }

  auto proto = std::make_unique<RecurrenceRankerProto>();
  if (!proto->ParseFromString(proto_str)) {
    LogSerializationStatus(model_identifier,
                           SerializationStatus::kFromProtoError);
    return nullptr;
  }

  LogSerializationStatus(model_identifier, SerializationStatus::kLoadOk);
  return proto;
}

std::vector<std::pair<std::string, float>> SortAndTruncateRanks(
    int n,
    const std::map<std::string, float>& ranks) {
  std::vector<std::pair<std::string, float>> sorted_ranks(ranks.begin(),
                                                          ranks.end());
  std::sort(sorted_ranks.begin(), sorted_ranks.end(),
            [](const std::pair<std::string, float>& a,
               const std::pair<std::string, float>& b) {
              return a.second > b.second;
            });

  // vector::resize simply truncates the array if there are more than n
  // elements. Note this is still O(N).
  if (sorted_ranks.size() > static_cast<size_t>(n))
    sorted_ranks.resize(n);
  return sorted_ranks;
}

// Given a FrecencyStore's map from target names to IDs, and a
// RecurrencePredictor's map of IDs to scores, returns a pair containing the
// following:
//
//  - A map from target names to scores.
//  - The proportion of IDs returned by the predictor that are 'valid', ie.
//    that exist in the target frecency store.
//
// The second value can be used to decide when to trigger a cleanup of the
// predictor's internal state.
std::pair<std::map<std::string, float>, float> ZipTargetsWithScores(
    const FrecencyStore::ScoreTable& target_to_id,
    const std::map<unsigned int, float>& id_to_score) {
  // Early exit if the predictor's ranks are empty. In this case make the
  // proportion of valid IDs 1.0, as a cleanup would be a noop.
  if (id_to_score.empty())
    return {{}, 1.0f};

  float num_valid_targets = 0.0f;
  std::map<std::string, float> target_to_score;
  for (const auto& pair : target_to_id) {
    DCHECK(pair.second.last_num_updates ==
           target_to_id.begin()->second.last_num_updates);
    const auto& it = id_to_score.find(pair.second.id);
    if (it != id_to_score.end()) {
      target_to_score[pair.first] = it->second;
      num_valid_targets += 1.0f;
    }
  }

  return {std::move(target_to_score), num_valid_targets / id_to_score.size()};
}

std::map<std::string, float> GetScoresFromFrecencyStore(
    const std::map<std::string, FrecencyStore::ValueData>& target_to_id) {
  std::map<std::string, float> target_to_score;
  for (const auto& pair : target_to_id) {
    DCHECK(pair.second.last_num_updates ==
           target_to_id.begin()->second.last_num_updates);
    target_to_score[pair.first] = pair.second.last_score;
  }
  return target_to_score;
}

}  // namespace

RecurrenceRanker::RecurrenceRanker(const std::string& model_identifier,
                                   const base::FilePath& filepath,
                                   const RecurrenceRankerConfigProto& config,
                                   bool is_ephemeral_user)
    : model_identifier_(model_identifier),
      proto_filepath_(filepath),
      config_hash_(base::PersistentHash(config.SerializeAsString())),
      is_ephemeral_user_(is_ephemeral_user),
      min_seconds_between_saves_(
          TimeDelta::FromSeconds(config.min_seconds_between_saves())),
      time_of_last_save_(Time::Now()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_ = base::CreateSequencedTaskRunner(
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  targets_ = std::make_unique<FrecencyStore>(config.target_limit(),
                                             config.target_decay());
  conditions_ = std::make_unique<FrecencyStore>(config.condition_limit(),
                                                config.condition_decay());

  if (is_ephemeral_user_) {
    // Ephemeral users have no persistent storage, so we don't try and load the
    // proto from disk. Instead, we fall back on using a default (frecency)
    // predictor, which is still useful with only data from the current session.
    LogInitializationStatus(model_identifier_,
                            InitializationStatus::kEphemeralUser);
    predictor_ = std::make_unique<DefaultPredictor>(
        RecurrencePredictorConfigProto::DefaultPredictorConfig(),
        model_identifier_);
  } else {
    predictor_ = MakePredictor(config.predictor(), model_identifier_);

    // Load the proto from disk and finish initialisation in
    // |OnLoadProtoFromDiskComplete|.
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&LoadProtoFromDisk, proto_filepath_, model_identifier_),
        base::BindOnce(&RecurrenceRanker::OnLoadProtoFromDiskComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

RecurrenceRanker::~RecurrenceRanker() = default;

void RecurrenceRanker::OnLoadProtoFromDiskComplete(
    std::unique_ptr<RecurrenceRankerProto> proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  load_from_disk_completed_ = true;
  LogInitializationStatus(model_identifier_,
                          InitializationStatus::kInitialized);

  // If OnLoadFromDisk returned nullptr, no saved ranker proto was available on
  // disk, and there is nothing to load. Save a blank ranker to prevent metrics
  // from reporting no ranker exists on future loads. Use SaveToDisk rather than
  // MaybeSave because the time of last save is set to the time at construction.
  if (!proto) {
    SaveToDisk();
    return;
  }

  if (!proto->has_config_hash() || proto->config_hash() != config_hash_) {
    // The configuration of the saved ranker doesn't match the configuration for
    // this object. We should not use any data from it, and instead start with a
    // clean slate. This is not always an error: it is expected if, for example,
    // a RecurrenceRanker instance is rolled out in one release, and then
    // reconfigured in the next.
    LogInitializationStatus(model_identifier_,
                            InitializationStatus::kHashMismatch);
    return;
  }

  if (proto->has_predictor())
    predictor_->FromProto(proto->predictor());
  else
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kPredictorMissingError);

  if (proto->has_targets())
    targets_->FromProto(proto->targets());
  else
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kTargetsMissingError);

  if (proto->has_conditions())
    conditions_->FromProto(proto->conditions());
  else
    LogSerializationStatus(model_identifier_,
                           SerializationStatus::kConditionsMissingError);
}

void RecurrenceRanker::Record(const std::string& target,
                              const std::string& condition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!load_from_disk_completed_)
    return;
  LogUsage(model_identifier_, Usage::kRecord);

  predictor_->Train(targets_->Update(target), conditions_->Update(condition));
  MaybeSave();
}

void RecurrenceRanker::RenameTarget(const std::string& target,
                                    const std::string& new_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!load_from_disk_completed_)
    return;
  LogUsage(model_identifier_, Usage::kRenameTarget);

  targets_->Rename(target, new_target);
  MaybeSave();
}

void RecurrenceRanker::RemoveTarget(const std::string& target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(tby): Find a solution to the edge case of a removal before disk
  // loading is complete, resulting in the remove getting dropped.
  if (!load_from_disk_completed_)
    return;
  LogUsage(model_identifier_, Usage::kRemoveTarget);

  targets_->Remove(target);
  MaybeSave();
}

void RecurrenceRanker::RenameCondition(const std::string& condition,
                                       const std::string& new_condition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!load_from_disk_completed_)
    return;
  LogUsage(model_identifier_, Usage::kRenameCondition);

  conditions_->Rename(condition, new_condition);
  MaybeSave();
}

void RecurrenceRanker::RemoveCondition(const std::string& condition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!load_from_disk_completed_)
    return;
  LogUsage(model_identifier_, Usage::kRemoveCondition);

  conditions_->Remove(condition);
  MaybeSave();
}

std::map<std::string, float> RecurrenceRanker::Rank(
    const std::string& condition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!load_from_disk_completed_)
    return {};
  LogUsage(model_identifier_, Usage::kRank);
  // Special case the default predictor, and return the scores from the target
  // frecency store.
  if (predictor_->GetPredictorName() == DefaultPredictor::kPredictorName)
    return GetScoresFromFrecencyStore(targets_->GetAll());

  base::Optional<unsigned int> condition_id = conditions_->GetId(condition);
  if (condition_id == base::nullopt)
    return {};

  const auto& targets = targets_->GetAll();
  const auto& zipped =
      ZipTargetsWithScores(targets, predictor_->Rank(condition_id.value()));
  MaybeCleanup(zipped.second, targets);
  return std::move(zipped.first);
}

void RecurrenceRanker::MaybeCleanup(float proportion_valid,
                                    const FrecencyStore::ScoreTable& targets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proportion_valid > kMinValidTargetProportionBeforeCleanup)
    return;

  std::vector<unsigned int> valid_targets;
  for (const auto& target_data : targets)
    valid_targets.push_back(target_data.second.id);
  predictor_->Cleanup(valid_targets);
}

std::vector<std::pair<std::string, float>> RecurrenceRanker::RankTopN(
    int n,
    const std::string& condition) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!load_from_disk_completed_)
    return {};

  return SortAndTruncateRanks(n, Rank(condition));
}

std::map<std::string, FrecencyStore::ValueData>*
RecurrenceRanker::GetTargetData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return targets_->get_mutable_values();
}

std::map<std::string, FrecencyStore::ValueData>*
RecurrenceRanker::GetConditionData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return conditions_->get_mutable_values();
}

void RecurrenceRanker::SaveToDisk() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_ephemeral_user_)
    return;

  time_of_last_save_ = Time::Now();
  RecurrenceRankerProto proto;
  ToProto(&proto);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&SaveProtoToDisk, proto_filepath_,
                                        model_identifier_, proto));
}

void RecurrenceRanker::MaybeSave() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (Time::Now() - time_of_last_save_ > min_seconds_between_saves_) {
    SaveToDisk();
  }
}

void RecurrenceRanker::ToProto(RecurrenceRankerProto* proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proto->set_config_hash(config_hash_);
  predictor_->ToProto(proto->mutable_predictor());
  targets_->ToProto(proto->mutable_targets());
  conditions_->ToProto(proto->mutable_conditions());
}

const char* RecurrenceRanker::GetPredictorNameForTesting() const {
  return predictor_->GetPredictorName();
}

}  // namespace app_list
