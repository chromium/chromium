// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_BATCH_ENTITY_METADATA_TASK_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_BATCH_ENTITY_METADATA_TASK_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace optimization_guide {

class EntityMetadataProvider;

// Callback to inform the caller that all requested entity metadata has been
// retrieved.
using BatchEntityMetadataRetrievedCallback = base::OnceCallback<void(
    const base::flat_map<std::string, optimization_guide::EntityMetadata>&)>;

// A task that fetches entity metadata for a batch of entity IDs.
class BatchEntityMetadataTask {
 public:
  BatchEntityMetadataTask(
      optimization_guide::EntityMetadataProvider* entity_metadata_provider,
      const base::flat_set<std::string>& entity_ids);
  BatchEntityMetadataTask(const BatchEntityMetadataTask&) = delete;
  BatchEntityMetadataTask& operator=(const BatchEntityMetadataTask&) = delete;
  ~BatchEntityMetadataTask();

  // Executes the task and invokes |callback| on completion.
  void Execute(BatchEntityMetadataRetrievedCallback callback);

 private:
  enum class TaskState {
    kWaiting,
    kStarted,
    kCompleted,
  };

  // Callback invoked when metadata for |entity_id| has been retrieved.
  void OnEntityMetadataRetrieved(
      const std::string& entity_id,
      const absl::optional<EntityMetadata>& entity_metadata);

  // The provider used to retrieve entity metadata from.
  raw_ptr<optimization_guide::EntityMetadataProvider> entity_metadata_provider_;
  // The entity IDs that metadata will be retrieved for.
  const base::flat_set<std::string> entity_ids_;
  // The state of this task. Mostly used for ensuring proper usage of |this|.
  TaskState task_state_ = TaskState::kWaiting;
  // The callback to invoke when metadata for all |entity_ids_| have been
  // received.
  BatchEntityMetadataRetrievedCallback callback_;
  // The entities for which EntityMetadata has been received for so far.
  base::flat_set<std::string> received_entity_ids_;
  // The mapping from entity ID to EntityMetadata that has been received so far.
  // Will be passed to |callback_| when all metadata has been received.
  base::flat_map<std::string, EntityMetadata> entity_metadata_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BatchEntityMetadataTask> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_BATCH_ENTITY_METADATA_TASK_H_