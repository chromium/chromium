// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/worker_context.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace resource_attribution {

WorkerContext::WorkerContext(const blink::WorkerToken& token,
                             base::WeakPtr<WorkerNode> weak_node)
    : token_(token), weak_node_(weak_node) {}

WorkerContext::~WorkerContext() = default;

WorkerContext::WorkerContext(const WorkerContext& other) = default;

WorkerContext& WorkerContext::operator=(const WorkerContext& other) = default;

WorkerContext::WorkerContext(WorkerContext&& other) = default;

WorkerContext& WorkerContext::operator=(WorkerContext&& other) = default;

// static
std::optional<WorkerContext> WorkerContext::FromWorkerToken(
    const blink::WorkerToken& token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<WorkerNode> worker_node =
      PerformanceManager::GetWorkerNodeForToken(token);
  if (!worker_node.MaybeValid()) {
    // This token was never seen by PerformanceManager.
    return std::nullopt;
  }
  return WorkerContext(token, std::move(worker_node));
}

blink::WorkerToken WorkerContext::GetWorkerToken() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return token_;
}

base::WeakPtr<WorkerNode> WorkerContext::GetWeakWorkerNode() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_node_;
}

// static
WorkerContext WorkerContext::FromWorkerNode(const WorkerNode* node) {
  CHECK(node);
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  auto* node_impl = WorkerNodeImpl::FromNode(node);
  return WorkerContext(node_impl->GetWorkerToken(), node_impl->GetWeakPtr());
}

// static
std::optional<WorkerContext> WorkerContext::FromWeakWorkerNode(
    base::WeakPtr<WorkerNode> node) {
  if (!node) {
    return std::nullopt;
  }
  return FromWorkerNode(node.get());
}

WorkerNode* WorkerContext::GetWorkerNode() const {
  if (weak_node_) {
    // `weak_node` will check anyway if dereferenced from the wrong sequence,
    // but let's be explicit.
    DCHECK_ON_GRAPH_SEQUENCE(weak_node_->GetGraph());
    return weak_node_.get();
  }
  return nullptr;
}

std::string WorkerContext::ToString() const {
  return base::StrCat({"WorkerContext:", token_.ToString()});
}

}  // namespace resource_attribution
