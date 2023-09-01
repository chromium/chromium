// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/resource_context_registry_storage.h"

#include <map>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

using UIThreadStorage = ResourceContextRegistryStorage::UIThreadStorage;

// Underlying storage for ResourceContext objects, accessible from the UI
// thread. The PM sequence holds a pointer to this to post tasks to it.
//
// For most of its lifetime this is owned on the PM sequence by
// ResourceContextRegistryStorage. The constructor sends a pointer to the UI
// thread, which stores it in `static_ui_thread_storage_`. The destructor passes
// ownership to the UI thread to delete, which clears
// `static_ui_thread_storage_`.
//
// Therefore the PM sequence can always post to UIThreadStorage while the
// ResourceContextRegistryStorage is in the graph, and the UI thread can always
// access UIThreadStorage while the `static_ui_thread_storage_` pointer is set.
//
// Static methods that don't need this storage should return nullopt if
// `static_ui_thread_storage_` is null. This guards against behaviour changes
// if the implementation is updated to use it.
class ResourceContextRegistryStorage::UIThreadStorage {
 public:
  UIThreadStorage() = default;
  ~UIThreadStorage() = default;

  UIThreadStorage(const UIThreadStorage&) = delete;
  UIThreadStorage& operator=(const UIThreadStorage&) = delete;

  // ProcessContext accessors.
  absl::optional<ProcessContext> GetBrowserProcessContext() const;
  absl::optional<ProcessContext> GetProcessContextForId(
      RenderProcessHostId id) const;
  absl::optional<ProcessContext> GetProcessContextForId(
      BrowserChildProcessHostId id) const;
  bool IsBrowserProcessContext(const ProcessContext& context) const;
  bool IsRenderProcessContext(const ProcessContext& context) const;
  bool IsBrowserChildProcessContext(const ProcessContext& context) const;
  content::RenderProcessHost* GetRenderProcessHostFromContext(
      const ProcessContext& context) const;
  content::BrowserChildProcessHost* GetBrowserChildProcessHostFromContext(
      const ProcessContext& context) const;

  // Update storage based on changes in the PM graph.
  void OnBrowserProcessNodeAdded(const ProcessContext& process_context);
  void OnRenderProcessNodeAdded(const ProcessContext& process_context,
                                const RenderProcessHostProxy& rph_proxy);
  void OnBrowserChildProcessNodeAdded(
      const ProcessContext& process_context,
      const BrowserChildProcessHostProxy& bcph_proxy);
  void OnBrowserProcessNodeRemoved(const ProcessContext& process_context);
  void OnRenderProcessNodeRemoved(const ProcessContext& process_context,
                                  const RenderProcessHostProxy& rph_proxy);
  void OnBrowserChildProcessNodeRemoved(
      const ProcessContext& process_context,
      const BrowserChildProcessHostProxy& bcph_proxy);

 private:
  // Asserts that `context` isn't in any map.
  void CheckProcessContextUnregistered(const ProcessContext& context) const;

  // ProcessContext storage
  absl::optional<ProcessContext> browser_process_context_;
  std::map<RenderProcessHostId, ProcessContext> process_contexts_by_rph_id_;
  std::map<BrowserChildProcessHostId, ProcessContext>
      process_contexts_by_bcph_id_;
  std::map<ProcessContext, RenderProcessHostId> rph_ids_by_process_context_;
  std::map<ProcessContext, BrowserChildProcessHostId>
      bcph_ids_by_process_context_;
};

absl::optional<ProcessContext> UIThreadStorage::GetBrowserProcessContext()
    const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_process_context_;
}

absl::optional<ProcessContext> UIThreadStorage::GetProcessContextForId(
    RenderProcessHostId id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = process_contexts_by_rph_id_.find(id);
  if (it == process_contexts_by_rph_id_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

absl::optional<ProcessContext> UIThreadStorage::GetProcessContextForId(
    BrowserChildProcessHostId id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = process_contexts_by_bcph_id_.find(id);
  if (it == process_contexts_by_bcph_id_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

bool UIThreadStorage::IsBrowserProcessContext(
    const ProcessContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_process_context_ == context;
}

bool UIThreadStorage::IsRenderProcessContext(
    const ProcessContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Contains(rph_ids_by_process_context_, context);
}

bool UIThreadStorage::IsBrowserChildProcessContext(
    const ProcessContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Contains(bcph_ids_by_process_context_, context);
}

content::RenderProcessHost* UIThreadStorage::GetRenderProcessHostFromContext(
    const ProcessContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = rph_ids_by_process_context_.find(context);
  return it == rph_ids_by_process_context_.end()
             ? nullptr
             : content::RenderProcessHost::FromID(it->second.GetUnsafeValue());
}

content::BrowserChildProcessHost*
UIThreadStorage::GetBrowserChildProcessHostFromContext(
    const ProcessContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = bcph_ids_by_process_context_.find(context);
  return it == bcph_ids_by_process_context_.end()
             ? nullptr
             : content::BrowserChildProcessHost::FromID(
                   it->second.GetUnsafeValue());
}

void UIThreadStorage::OnBrowserProcessNodeAdded(
    const ProcessContext& process_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CheckProcessContextUnregistered(process_context);
  browser_process_context_ = process_context;
}

void UIThreadStorage::OnRenderProcessNodeAdded(
    const ProcessContext& process_context,
    const RenderProcessHostProxy& rph_proxy) {
  CheckProcessContextUnregistered(process_context);
  process_contexts_by_rph_id_.emplace(rph_proxy.render_process_host_id(),
                                      process_context);
  rph_ids_by_process_context_.emplace(process_context,
                                      rph_proxy.render_process_host_id());
}

void UIThreadStorage::OnBrowserChildProcessNodeAdded(
    const ProcessContext& process_context,
    const BrowserChildProcessHostProxy& bcph_proxy) {
  CheckProcessContextUnregistered(process_context);
  process_contexts_by_bcph_id_.emplace(
      bcph_proxy.browser_child_process_host_id(), process_context);
  bcph_ids_by_process_context_.emplace(
      process_context, bcph_proxy.browser_child_process_host_id());
}

void UIThreadStorage::OnBrowserProcessNodeRemoved(
    const ProcessContext& process_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_EQ(browser_process_context_.value(), process_context);
  browser_process_context_.reset();
}

void UIThreadStorage::OnRenderProcessNodeRemoved(
    const ProcessContext& process_context,
    const RenderProcessHostProxy& rph_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto context_it =
      process_contexts_by_rph_id_.find(rph_proxy.render_process_host_id());
  CHECK(context_it != process_contexts_by_rph_id_.end());
  CHECK_EQ(context_it->second, process_context);
  process_contexts_by_rph_id_.erase(context_it);

  const auto rph_it = rph_ids_by_process_context_.find(process_context);
  CHECK(rph_it != rph_ids_by_process_context_.end());
  CHECK_EQ(rph_it->second, rph_proxy.render_process_host_id());
  rph_ids_by_process_context_.erase(rph_it);
}

void UIThreadStorage::OnBrowserChildProcessNodeRemoved(
    const ProcessContext& process_context,
    const BrowserChildProcessHostProxy& bcph_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const auto context_it = process_contexts_by_bcph_id_.find(
      bcph_proxy.browser_child_process_host_id());
  CHECK(context_it != process_contexts_by_bcph_id_.end());
  CHECK_EQ(context_it->second, process_context);
  process_contexts_by_bcph_id_.erase(context_it);

  const auto bcph_it = bcph_ids_by_process_context_.find(process_context);
  CHECK(bcph_it != bcph_ids_by_process_context_.end());
  CHECK_EQ(bcph_it->second, bcph_proxy.browser_child_process_host_id());
  bcph_ids_by_process_context_.erase(bcph_it);
}

void UIThreadStorage::CheckProcessContextUnregistered(
    const ProcessContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser_process_context_ != context);
  CHECK(!base::Contains(rph_ids_by_process_context_, context));
  CHECK(!base::Contains(bcph_ids_by_process_context_, context));
}

UIThreadStorage* ResourceContextRegistryStorage::static_ui_thread_storage_ =
    nullptr;

ResourceContextRegistryStorage::ResourceContextRegistryStorage()
    : ui_thread_storage_(std::make_unique<UIThreadStorage>()) {
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RegisterUIThreadStorage, ui_thread_storage_.get()));
}

ResourceContextRegistryStorage::~ResourceContextRegistryStorage() {
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeleteUIThreadStorage, std::move(ui_thread_storage_)));
}

// static
absl::optional<ProcessContext>
ResourceContextRegistryStorage::BrowserProcessContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (static_ui_thread_storage_) {
    return static_ui_thread_storage_->GetBrowserProcessContext();
  }
  return absl::nullopt;
}

// static
absl::optional<ProcessContext>
ResourceContextRegistryStorage::ProcessContextForId(RenderProcessHostId id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (static_ui_thread_storage_) {
    return static_ui_thread_storage_->GetProcessContextForId(id);
  }
  return absl::nullopt;
}

// static
absl::optional<ProcessContext>
ResourceContextRegistryStorage::ProcessContextForId(
    BrowserChildProcessHostId id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (static_ui_thread_storage_) {
    return static_ui_thread_storage_->GetProcessContextForId(id);
  }
  return absl::nullopt;
}

// static
bool ResourceContextRegistryStorage::IsBrowserProcessContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->IsBrowserProcessContext(context)
             : false;
}

// static
bool ResourceContextRegistryStorage::IsRenderProcessContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->IsRenderProcessContext(context)
             : false;
}

// static
bool ResourceContextRegistryStorage::IsBrowserChildProcessContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->IsBrowserChildProcessContext(context)
             : false;
}

// static
content::RenderProcessHost*
ResourceContextRegistryStorage::RenderProcessHostFromContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->GetRenderProcessHostFromContext(
                   context)
             : nullptr;
}

// static
content::BrowserChildProcessHost*
ResourceContextRegistryStorage::BrowserChildProcessHostFromContext(
    const ProcessContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->GetBrowserChildProcessHostFromContext(
                   context)
             : nullptr;
}

const ProcessNode* ResourceContextRegistryStorage::GetProcessNodeForContext(
    const ProcessContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = process_nodes_by_context_.find(context);
  return it != process_nodes_by_context_.end() ? it->second : nullptr;
}

void ResourceContextRegistryStorage::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  ProcessContext process_context = process_node->GetResourceContext();
  // Unretained is safe because `ui_thread_storage_` is passed to the UI thread
  // to delete.
  switch (process_node->GetProcessType()) {
    case content::PROCESS_TYPE_BROWSER:
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&UIThreadStorage::OnBrowserProcessNodeAdded,
                                    base::Unretained(ui_thread_storage_.get()),
                                    process_context));
      break;
    case content::PROCESS_TYPE_RENDERER:
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&UIThreadStorage::OnRenderProcessNodeAdded,
                                    base::Unretained(ui_thread_storage_.get()),
                                    process_context,
                                    process_node->GetRenderProcessHostProxy()));
      break;
    default:
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&UIThreadStorage::OnBrowserChildProcessNodeAdded,
                         base::Unretained(ui_thread_storage_.get()),
                         process_context,
                         process_node->GetBrowserChildProcessHostProxy()));
      break;
  }
  const auto [_, inserted] = process_nodes_by_context_.emplace(
      std::move(process_context), process_node);
  CHECK(inserted);
}

void ResourceContextRegistryStorage::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  const ProcessContext& process_context = process_node->GetResourceContext();
  const size_t erased = process_nodes_by_context_.erase(process_context);
  CHECK_EQ(erased, 1u);
  // Unretained is safe because `ui_thread_storage_` is passed to the UI thread
  // to delete.
  switch (process_node->GetProcessType()) {
    case content::PROCESS_TYPE_BROWSER:
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&UIThreadStorage::OnBrowserProcessNodeRemoved,
                         base::Unretained(ui_thread_storage_.get()),
                         process_context));
      break;
    case content::PROCESS_TYPE_RENDERER:
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&UIThreadStorage::OnRenderProcessNodeRemoved,
                         base::Unretained(ui_thread_storage_.get()),
                         process_context,
                         process_node->GetRenderProcessHostProxy()));
      break;
    default:
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&UIThreadStorage::OnBrowserChildProcessNodeRemoved,
                         base::Unretained(ui_thread_storage_.get()),
                         process_context,
                         process_node->GetBrowserChildProcessHostProxy()));
      break;
  }
}

void ResourceContextRegistryStorage::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RegisterObject(&process_registry_);
  graph->AddProcessNodeObserver(this);
}

void ResourceContextRegistryStorage::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveProcessNodeObserver(this);
  graph->UnregisterObject(&process_registry_);
}

// static
void ResourceContextRegistryStorage::RegisterUIThreadStorage(
    UIThreadStorage* storage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!static_ui_thread_storage_);
  static_ui_thread_storage_ = storage;
}

// static
void ResourceContextRegistryStorage::DeleteUIThreadStorage(
    std::unique_ptr<UIThreadStorage> storage) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_EQ(storage.get(), static_ui_thread_storage_);
  static_ui_thread_storage_ = nullptr;
}

}  // namespace performance_manager::resource_attribution
