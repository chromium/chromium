// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/execution/default_model_manager.h"

#include "base/task/single_thread_task_runner.h"
#include "components/segmentation_platform/internal/database/segment_info_database.h"

namespace segmentation_platform {

DefaultModelManager::SegmentInfoWrapper::SegmentInfoWrapper() = default;
DefaultModelManager::SegmentInfoWrapper::~SegmentInfoWrapper() = default;

DefaultModelManager::DefaultModelManager(
    ModelProviderFactory* model_provider_factory,
    const base::flat_set<SegmentId>& segment_ids)
    : model_provider_factory_(model_provider_factory) {
  for (SegmentId segment_id : segment_ids) {
    std::unique_ptr<DefaultModelProvider> provider =
        model_provider_factory->CreateDefaultProvider(segment_id);
    if (!provider)
      continue;
    default_model_providers_.emplace(segment_id, std::move(provider));
  }
}

DefaultModelManager::~DefaultModelManager() = default;

DefaultModelProvider* DefaultModelManager::GetDefaultProvider(
    SegmentId segment_id) {
  auto it = default_model_providers_.find(segment_id);
  if (it != default_model_providers_.end())
    return it->second.get();
  return nullptr;
}

void DefaultModelManager::GetAllSegmentInfoFromDefaultModel(
    const base::flat_set<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback) {
  auto result = std::make_unique<SegmentInfoList>();
  std::deque<SegmentId> remaining_segment_ids(segment_ids.begin(),
                                              segment_ids.end());
  GetNextSegmentInfoFromDefaultModel(
      std::move(result), std::move(remaining_segment_ids), std::move(callback));
}

void DefaultModelManager::GetNextSegmentInfoFromDefaultModel(
    std::unique_ptr<SegmentInfoList> result,
    std::deque<SegmentId> remaining_segment_ids,
    MultipleSegmentInfoCallback callback) {
  SegmentId segment_id = SegmentId::OPTIMIZATION_TARGET_UNKNOWN;
  DefaultModelProvider* default_provider = nullptr;

  // Find the next available default provider.
  while (!default_provider && !remaining_segment_ids.empty()) {
    segment_id = remaining_segment_ids.front();
    remaining_segment_ids.pop_front();
    if (default_model_providers_.count(segment_id) == 1) {
      default_provider = default_model_providers_[segment_id].get();
      break;
    }
  }

  if (!default_provider) {
    // If there are no more default providers, return the result so far.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(*result)));
    return;
  }

  // TODO(ritikagup): Change use of InitAndFetch() to GetModelConfig().
  ModelProvider* base_class = default_provider;
  base_class->InitAndFetchModel(base::BindRepeating(
      &DefaultModelManager::OnFetchDefaultModel, weak_ptr_factory_.GetWeakPtr(),
      base::Passed(&result), remaining_segment_ids, base::Passed(&callback)));
}

void DefaultModelManager::OnFetchDefaultModel(
    std::unique_ptr<SegmentInfoList> result,
    std::deque<SegmentId> remaining_segment_ids,
    MultipleSegmentInfoCallback callback,
    SegmentId segment_id,
    proto::SegmentationModelMetadata metadata,
    int64_t model_version) {
  auto info = std::make_unique<SegmentInfoWrapper>();
  info->segment_source = DefaultModelManager::SegmentSource::DEFAULT_MODEL;
  info->segment_info.set_segment_id(segment_id);
  info->segment_info.mutable_model_metadata()->CopyFrom(metadata);
  info->segment_info.set_model_version(model_version);
  info->segment_info.set_model_source(proto::ModelSource::DEFAULT_MODEL_SOURCE);
  result->push_back(std::move(info));

  GetNextSegmentInfoFromDefaultModel(
      std::move(result), std::move(remaining_segment_ids), std::move(callback));
}

void DefaultModelManager::GetAllSegmentInfoFromBothModels(
    const base::flat_set<SegmentId>& segment_ids,
    SegmentInfoDatabase* segment_database,
    MultipleSegmentInfoCallback callback) {
  segment_database->GetSegmentInfoForSegments(
      segment_ids,
      base::BindOnce(&DefaultModelManager::OnGetAllSegmentInfoFromDatabase,
                     weak_ptr_factory_.GetWeakPtr(), segment_ids,
                     std::move(callback)));
}

void DefaultModelManager::OnGetAllSegmentInfoFromDatabase(
    const base::flat_set<SegmentId>& segment_ids,
    MultipleSegmentInfoCallback callback,
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos) {
  GetAllSegmentInfoFromDefaultModel(
      segment_ids,
      base::BindOnce(&DefaultModelManager::OnGetAllSegmentInfoFromDefaultModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(segment_infos)));
}

void DefaultModelManager::OnGetAllSegmentInfoFromDefaultModel(
    MultipleSegmentInfoCallback callback,
    std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> segment_infos_from_db,
    SegmentInfoList segment_infos_from_default_model) {
  SegmentInfoList merged_results;
  if (segment_infos_from_db) {
    for (auto it : *segment_infos_from_db) {
      merged_results.push_back(std::make_unique<SegmentInfoWrapper>());
      merged_results.back()->segment_source = SegmentSource::DATABASE;
      merged_results.back()->segment_info.Swap(&it.second);
    }
  }
  merged_results.insert(
      merged_results.end(),
      std::make_move_iterator(segment_infos_from_default_model.begin()),
      std::make_move_iterator(segment_infos_from_default_model.end()));

  std::move(callback).Run(std::move(merged_results));
}

void DefaultModelManager::SetDefaultProvidersForTesting(
    std::map<SegmentId, std::unique_ptr<DefaultModelProvider>>&& providers) {
  default_model_providers_ = std::move(providers);
}

}  // namespace segmentation_platform
