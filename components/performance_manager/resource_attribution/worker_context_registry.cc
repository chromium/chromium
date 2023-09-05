// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/worker_context_registry.h"

#include "base/sequence_checker.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/resource_attribution/resource_context_registry_storage.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager::resource_attribution {

WorkerContextRegistry::WorkerContextRegistry(
    const ResourceContextRegistryStorage& storage)
    : storage_(storage) {}

WorkerContextRegistry::~WorkerContextRegistry() = default;

// static
absl::optional<WorkerContext> WorkerContextRegistry::ContextForWorkerToken(
    const blink::WorkerToken& token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::WorkerContextForWorkerToken(token);
}

// static
absl::optional<blink::WorkerToken>
WorkerContextRegistry::WorkerTokenFromContext(const WorkerContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ResourceContextRegistryStorage::WorkerTokenFromContext(context);
}

// static
absl::optional<blink::WorkerToken>
WorkerContextRegistry::WorkerTokenFromContext(const ResourceContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ContextIs<WorkerContext>(context)) {
    return ResourceContextRegistryStorage::WorkerTokenFromContext(
        AsContext<WorkerContext>(context));
  }
  return absl::nullopt;
}

const WorkerNode* WorkerContextRegistry::GetWorkerNodeForContext(
    const WorkerContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_->GetWorkerNodeForContext(context);
}

const WorkerNode* WorkerContextRegistry::GetWorkerNodeForContext(
    const ResourceContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ContextIs<WorkerContext>(context)
             ? storage_->GetWorkerNodeForContext(
                   AsContext<WorkerContext>(context))
             : nullptr;
}

}  // namespace performance_manager::resource_attribution
