// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/process_context.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/resource_attribution/performance_manager_aliases.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace resource_attribution {

ProcessContext::ProcessContext(AnyProcessHostId id,
                               base::WeakPtr<ProcessNode> weak_node)
    : id_(id), weak_node_(weak_node) {}

ProcessContext::~ProcessContext() = default;

ProcessContext::ProcessContext(const ProcessContext& other) = default;

ProcessContext& ProcessContext::operator=(const ProcessContext& other) =
    default;

ProcessContext::ProcessContext(ProcessContext&& other) = default;

ProcessContext& ProcessContext::operator=(ProcessContext&& other) = default;

// static
std::optional<ProcessContext> ProcessContext::FromBrowserProcess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForBrowserProcess();
  if (!process_node.MaybeValid()) {
    return std::nullopt;
  }
  return ProcessContext(BrowserProcessTag{}, std::move(process_node));
}

// static
std::optional<ProcessContext> ProcessContext::FromRenderProcessHost(
    content::RenderProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(host);
  RenderProcessHostId id(host->GetID());
  CHECK(!id.is_null());
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(host);
  if (!process_node.MaybeValid()) {
    return std::nullopt;
  }
  return ProcessContext(std::move(id), std::move(process_node));
}

// static
std::optional<ProcessContext> ProcessContext::FromBrowserChildProcessHost(
    content::BrowserChildProcessHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(host);
  BrowserChildProcessHostId id(host->GetData().id);
  CHECK(!id.is_null());
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForBrowserChildProcessHost(host);
  if (!process_node.MaybeValid()) {
    return std::nullopt;
  }
  return ProcessContext(std::move(id), std::move(process_node));
}

bool ProcessContext::IsBrowserProcessContext() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return absl::holds_alternative<BrowserProcessTag>(id_);
}

bool ProcessContext::IsRenderProcessContext() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return absl::holds_alternative<RenderProcessHostId>(id_);
}

bool ProcessContext::IsBrowserChildProcessContext() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return absl::holds_alternative<BrowserChildProcessHostId>(id_);
}

content::RenderProcessHost* ProcessContext::GetRenderProcessHost() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto* rph_id = absl::get_if<RenderProcessHostId>(&id_);
  return rph_id ? content::RenderProcessHost::FromID(rph_id->GetUnsafeValue())
                : nullptr;
}

RenderProcessHostId ProcessContext::GetRenderProcessHostId() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto* rph_id = absl::get_if<RenderProcessHostId>(&id_);
  return rph_id ? *rph_id : RenderProcessHostId();
}

content::BrowserChildProcessHost* ProcessContext::GetBrowserChildProcessHost()
    const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto* bcph_id = absl::get_if<BrowserChildProcessHostId>(&id_);
  return bcph_id ? content::BrowserChildProcessHost::FromID(
                       bcph_id->GetUnsafeValue())
                 : nullptr;
}

BrowserChildProcessHostId ProcessContext::GetBrowserChildProcessHostId() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto* bcph_id = absl::get_if<BrowserChildProcessHostId>(&id_);
  return bcph_id ? *bcph_id : BrowserChildProcessHostId();
}

base::WeakPtr<ProcessNode> ProcessContext::GetWeakProcessNode() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_node_;
}

// static
ProcessContext ProcessContext::FromProcessNode(const ProcessNode* node) {
  CHECK(node);
  DCHECK_ON_GRAPH_SEQUENCE(node->GetGraph());
  auto* node_impl = ProcessNodeImpl::FromNode(node);
  AnyProcessHostId id;
  switch (node_impl->GetProcessType()) {
    case content::PROCESS_TYPE_BROWSER:
      id = BrowserProcessTag{};
      break;
    case content::PROCESS_TYPE_RENDERER:
      id = node_impl->GetRenderProcessHostId();
      CHECK(!absl::get<RenderProcessHostId>(id).is_null());
      break;
    default:
      id = node_impl->GetBrowserChildProcessHostProxy()
               .browser_child_process_host_id();
      CHECK(!absl::get<BrowserChildProcessHostId>(id).is_null());
      break;
  }
  return ProcessContext(std::move(id), node_impl->GetWeakPtr());
}

// static
std::optional<ProcessContext> ProcessContext::FromWeakProcessNode(
    base::WeakPtr<ProcessNode> node) {
  if (!node) {
    return std::nullopt;
  }
  return FromProcessNode(node.get());
}

ProcessNode* ProcessContext::GetProcessNode() const {
  if (weak_node_) {
    // `weak_node` will check anyway if dereferenced from the wrong sequence,
    // but let's be explicit.
    DCHECK_ON_GRAPH_SEQUENCE(weak_node_->GetGraph());
    return weak_node_.get();
  }
  return nullptr;
}

std::string ProcessContext::ToString() const {
  return absl::visit(
      base::Overloaded{
          [](const BrowserProcessTag&) -> std::string {
            return "ProcessContext:Browser";
          },
          [](const RenderProcessHostId& id) -> std::string {
            return base::StrCat({"ProcessContext:Renderer:",
                                 base::NumberToString(id.GetUnsafeValue())});
          },
          [](const BrowserChildProcessHostId& id) -> std::string {
            return base::StrCat({"ProcessContext:BrowserChild:",
                                 base::NumberToString(id.GetUnsafeValue())});
          },
      },
      id_);
}

}  // namespace resource_attribution
