// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/frame_context.h"

#include <optional>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"

namespace resource_attribution {

namespace {

bool IsValidId(content::GlobalRenderFrameHostId id) {
  return !RenderProcessHostId(id.child_id).is_null() &&
         id.frame_routing_id != MSG_ROUTING_NONE;
}

}  // namespace

FrameContext::FrameContext(content::GlobalRenderFrameHostId id,
                           base::WeakPtr<FrameNode> weak_node)
    : id_(id), weak_node_(weak_node) {}

FrameContext::~FrameContext() = default;

FrameContext::FrameContext(const FrameContext& other) = default;

FrameContext& FrameContext::operator=(const FrameContext& other) = default;

FrameContext::FrameContext(FrameContext&& other) = default;

FrameContext& FrameContext::operator=(FrameContext&& other) = default;

// static
std::optional<FrameContext> FrameContext::FromRenderFrameHost(
    content::RenderFrameHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(host);
  CHECK(IsValidId(host->GetGlobalId()));
  base::WeakPtr<FrameNode> frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(host);
  if (!frame_node.MaybeValid()) {
    // GetFrameNodeForRenderFrameHost returns an explicit null WeakPtr if
    // PerformanceManager is not started or `host` has no FrameNode yet. In this
    // case MaybeValid() is guaranteed false and this should return nullopt.
    //
    // Otherwise this should return a FrameContext containing a WeakPtr, which
    // will become invalid once the FrameNode dies. FrameNode outlives the
    // RenderFrameHost, and the caller had a valid RenderFrameHost on the UI
    // thread, so the WeakPtr is guaranteed to be valid at this point. Therefore
    // MaybeValid() can only be false in the first case.
    return std::nullopt;
  }
  return FrameContext(host->GetGlobalId(), std::move(frame_node));
}

content::RenderFrameHost* FrameContext::GetRenderFrameHost() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return content::RenderFrameHost::FromID(id_);
}

content::GlobalRenderFrameHostId FrameContext::GetRenderFrameHostId() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return id_;
}

base::WeakPtr<FrameNode> FrameContext::GetWeakFrameNode() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_node_;
}

// static
FrameContext FrameContext::FromFrameNode(const FrameNode* node) {
  CHECK(node);
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  auto* node_impl = FrameNodeImpl::FromNode(node);
  CHECK(node_impl->process_node());
  content::GlobalRenderFrameHostId global_id(
      node_impl->process_node()->GetRenderProcessHostId().GetUnsafeValue(),
      node_impl->render_frame_id());
  CHECK(IsValidId(global_id));
  return FrameContext(global_id, node_impl->GetWeakPtr());
}

// static
std::optional<FrameContext> FrameContext::FromWeakFrameNode(
    base::WeakPtr<FrameNode> node) {
  if (!node) {
    return std::nullopt;
  }
  return FromFrameNode(node.get());
}

FrameNode* FrameContext::GetFrameNode() const {
  if (weak_node_) {
    // `weak_node` will check anyway if dereferenced from the wrong sequence,
    // but let's be explicit.
    DCHECK_ON_GRAPH_SEQUENCE(weak_node_->GetGraph());
    return weak_node_.get();
  }
  return nullptr;
}

std::string FrameContext::ToString() const {
  // Using stringstream instead of StrCat because `id_` has a streaming
  // operator.
  std::stringstream s;
  s << "FrameContext:" << id_;
  return s.str();
}

}  // namespace resource_attribution
