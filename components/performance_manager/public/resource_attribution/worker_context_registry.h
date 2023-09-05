// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_WORKER_CONTEXT_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_WORKER_CONTEXT_REGISTRY_H_

#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager::resource_attribution {

class ResourceContextRegistryStorage;

class WorkerContextRegistry final
    : public GraphRegisteredImpl<WorkerContextRegistry> {
 public:
  WorkerContextRegistry(const WorkerContextRegistry&) = delete;
  WorkerContextRegistry& operator=(const WorkerContextRegistry&) = delete;

  // Accessors to look up WorkerContext tokens on the UI thread. These are
  // always safe to call but will return nullopt if the PerformanceManager graph
  // is not initialized (during startup and shutdown).
  static absl::optional<WorkerContext> ContextForWorkerToken(
      const blink::WorkerToken& token);

  // Accessors to resolve WorkerContext tokens on the UI thread. These are
  // always safe to call but will always return false/nullopt if the
  // PerformanceManager graph is not initialized (during startup and shutdown).
  static absl::optional<blink::WorkerToken> WorkerTokenFromContext(
      const WorkerContext& context);
  static absl::optional<blink::WorkerToken> WorkerTokenFromContext(
      const ResourceContext& context);

  // Accessors to resolve WorkerContext tokens on the PM sequence.
  const WorkerNode* GetWorkerNodeForContext(const WorkerContext& context) const;
  const WorkerNode* GetWorkerNodeForContext(
      const ResourceContext& context) const;

 private:
  friend class ResourceContextRegistryStorage;

  explicit WorkerContextRegistry(const ResourceContextRegistryStorage& storage);
  ~WorkerContextRegistry() final;

  // Validates that non-static methods are called on the PM sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Accessor for registry storage on the PM sequence.
  raw_ref<const ResourceContextRegistryStorage> storage_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_WORKER_CONTEXT_REGISTRY_H_
