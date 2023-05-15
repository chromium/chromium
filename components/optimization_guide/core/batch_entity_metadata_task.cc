// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/batch_entity_metadata_task.h"

#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

namespace optimization_guide {

BatchEntityMetadataTask::BatchEntityMetadataTask(
    optimization_guide::EntityMetadataProvider* entity_metadata_provider,
    const base::flat_set<std::string>& entity_ids)
    : entity_metadata_provider_(entity_metadata_provider),
      entity_ids_(entity_ids) {
  DCHECK(entity_metadata_provider_);
  DCHECK(!entity_ids_.empty());
}

BatchEntityMetadataTask::~BatchEntityMetadataTask() = default;

void BatchEntityMetadataTask::Execute(
    BatchEntityMetadataRetrievedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(task_state_, TaskState::kWaiting);
  task_state_ = TaskState::kStarted;

  callback_ = std::move(callback);
  for (const auto& entity_id : entity_ids_) {
    entity_metadata_provider_->GetMetadataForEntityId(
        entity_id,
        base::BindOnce(&BatchEntityMetadataTask::OnEntityMetadataRetrieved,
                       weak_ptr_factory_.GetWeakPtr(), entity_id));
  }
}

void BatchEntityMetadataTask::OnEntityMetadataRetrieved(
    const std::string& entity_id,
    const absl::optional<EntityMetadata>& entity_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(task_state_, TaskState::kStarted);

  DCHECK(received_entity_ids_.find(entity_id) == received_entity_ids_.end());
  received_entity_ids_.insert(entity_id);

  if (entity_metadata)
    entity_metadata_map_.insert({entity_id, *entity_metadata});

  // Run callback if all the metadata has come back.
  if (received_entity_ids_.size() == entity_ids_.size()) {
    task_state_ = TaskState::kCompleted;
    std::move(callback_).Run(entity_metadata_map_);
  }
}

void BatchEntityMetadataTask::OnBatchEntityMetadataRetrieved(
    const base::flat_map<std::string, EntityMetadata>& entity_metadata_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(task_state_, TaskState::kStarted);

  task_state_ = TaskState::kCompleted;
  std::move(callback_).Run(entity_metadata_map);
}

}  // namespace optimization_guide
