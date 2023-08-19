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

void DefaultModelManager::GetAllSegmentInfoFromBothModels(
    const base::flat_set<SegmentId>& segment_ids,
    SegmentInfoDatabase* segment_database,
    MultipleSegmentInfoCallback callback) {
  std::unique_ptr<SegmentInfoDatabase::SegmentInfoList> available_segments =
      segment_database->GetSegmentInfoForBothModels(segment_ids);

  SegmentInfoList results;
  for (auto it : *available_segments) {
    results.push_back(std::make_unique<SegmentInfoWrapper>());
    results.back()->segment_source =
        it.second.model_source() == proto::ModelSource::DEFAULT_MODEL_SOURCE
            ? SegmentSource::DEFAULT_MODEL
            : SegmentSource::DATABASE;
    results.back()->segment_info.Swap(&it.second);
  }
  std::move(callback).Run(std::move(results));
}

void DefaultModelManager::SetDefaultProvidersForTesting(
    std::map<SegmentId, std::unique_ptr<DefaultModelProvider>>&& providers) {
  default_model_providers_ = std::move(providers);
}

}  // namespace segmentation_platform
