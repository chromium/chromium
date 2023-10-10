// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/resource_context_registry_storage.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::resource_attribution {

namespace {

// Helper to return a node from a map of ResourceContext -> WeakPtr, and clear
// any invalidated WeakPtr from the map.
template <typename NodeType, typename ContextType>
const NodeType* GetWeakNodeForContext(
    const ContextType& context,
    std::map<ContextType, base::WeakPtr<NodeType>>& weak_nodes_by_context) {
  const auto it = weak_nodes_by_context.find(context);
  if (it == weak_nodes_by_context.end()) {
    return nullptr;
  }
  if (!it->second) {
    // Clear invalidated WeakPtr from map.
    weak_nodes_by_context.erase(it);
    return nullptr;
  }
  return it->second.get();
}

}  // namespace

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

  // PageContext accessors.
  absl::optional<PageContext> GetPageContextForId(
      const content::GlobalRenderFrameHostId& id) const;
  content::WebContents* GetWebContentsFromContext(
      const PageContext& context) const;
  content::RenderFrameHost* GetCurrentMainRenderFrameHostFromContext(
      const PageContext& context) const;
  std::set<content::RenderFrameHost*> GetAllMainRenderFrameHostsFromContext(
      const PageContext& context) const;

  // Update storage based on changes in the PM graph.

  // Called when a FrameNode belonging to the page `page_context` is added to
  // the PM graph.
  void OnFrameNodeAdded(const PageContext& page_context,
                        const RenderFrameHostProxy& rfh_proxy,
                        bool is_main_frame,
                        bool is_current);

  // Called when a FrameNode belonging to the page `page_context` is removed
  // from the PM graph.
  void OnFrameNodeRemoved(const PageContext& page_context,
                          const RenderFrameHostProxy& rfh_proxy,
                          bool is_main_frame);

  // Called when the frame in RenderFrameHost `rfh_proxy` becomes the current
  // main frame of page `page_context` (if `is_current` is true) or stops being
  // the current main frame (if `is_current` is false).
  void OnCurrentMainFrameChanged(const PageContext& page_context,
                                 const RenderFrameHostProxy& rfh_proxy,
                                 bool is_current);

  void OnPageNodeAdded(const PageContext& page_context, WebContentsProxy proxy);

  void OnPageNodeRemoved(const PageContext& page_context);

 private:
  // PageContext storage

  std::map<PageContext, WebContentsProxy> web_contents_by_page_context_;

  // Map from context to the PageNode's main frames. Each is stored with its
  // most recent value of FrameNode::IsCurrent(). A PageNode can have several
  // main FrameNode's for different page states (active, prerendering, etc.) but
  // only one will be "current".
  //
  // TODO(crbug.com/1211368): Change the interface to use the "active" frame
  // node, using RenderFrameHost::GetLifecycleState. For now this is tracking
  // the behaviour of Performance Manager so at least it's consistent, but the
  // PM behaviour doesn't make as much sense from the WebContents perspective.
  // See the comment in FrameNodeImpl::SetIsCurrent.
  std::map<PageContext, std::map<content::GlobalRenderFrameHostId, bool>>
      main_rfh_ids_by_page_context_;

  // Map from every frame to the context of the PageNode containing it.
  std::map<content::GlobalRenderFrameHostId, PageContext>
      page_contexts_by_rfh_id_;
};

absl::optional<PageContext> UIThreadStorage::GetPageContextForId(
    const content::GlobalRenderFrameHostId& id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = page_contexts_by_rfh_id_.find(id);
  if (it == page_contexts_by_rfh_id_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

content::WebContents* UIThreadStorage::GetWebContentsFromContext(
    const PageContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = web_contents_by_page_context_.find(context);
  if (it == web_contents_by_page_context_.end()) {
    return nullptr;
  }
  return it->second.Get();
}

content::RenderFrameHost*
UIThreadStorage::GetCurrentMainRenderFrameHostFromContext(
    const PageContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = main_rfh_ids_by_page_context_.find(context);
  if (it == main_rfh_ids_by_page_context_.end()) {
    return nullptr;
  }
  // Iterate over all the PageNode's main frames, and return the one which is
  // current. If none are current, return any of them.
  content::RenderFrameHost* any_host = nullptr;
  for (const auto [rfh_id, is_current] : it->second) {
    if (is_current) {
      return content::RenderFrameHost::FromID(rfh_id);
    }
    if (!any_host) {
      any_host = content::RenderFrameHost::FromID(rfh_id);
    }
  }
  return any_host;
}

std::set<content::RenderFrameHost*>
UIThreadStorage::GetAllMainRenderFrameHostsFromContext(
    const PageContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::set<content::RenderFrameHost*> hosts;
  const auto it = main_rfh_ids_by_page_context_.find(context);
  if (it == main_rfh_ids_by_page_context_.end()) {
    return hosts;
  }
  for (const auto [rfh_id, _] : it->second) {
    auto* host = content::RenderFrameHost::FromID(rfh_id);
    if (host) {
      hosts.insert(host);
    }
  }
  return hosts;
}

void UIThreadStorage::OnFrameNodeAdded(const PageContext& page_context,
                                       const RenderFrameHostProxy& rfh_proxy,
                                       bool is_main_frame,
                                       bool is_current) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check OnPageNodeAdded() has recorded `page_context`. If not, PM is sending
  // FrameNode notifications before the containing PageNode notifications.
  CHECK(base::Contains(web_contents_by_page_context_, page_context));

  const auto [_1, frame_inserted] = page_contexts_by_rfh_id_.emplace(
      rfh_proxy.global_frame_routing_id(), page_context);
  CHECK(frame_inserted);
  if (is_main_frame) {
    std::map<content::GlobalRenderFrameHostId, bool>& main_frames =
        main_rfh_ids_by_page_context_[page_context];
    const auto [_2, context_inserted] =
        main_frames.emplace(rfh_proxy.global_frame_routing_id(), is_current);
    CHECK(context_inserted);
  }
}

void UIThreadStorage::OnFrameNodeRemoved(const PageContext& page_context,
                                         const RenderFrameHostProxy& rfh_proxy,
                                         bool is_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_main_frame) {
    const auto main_rfh_id_iter =
        main_rfh_ids_by_page_context_.find(page_context);
    CHECK(main_rfh_id_iter != main_rfh_ids_by_page_context_.end());
    const size_t erased_frames =
        main_rfh_id_iter->second.erase(rfh_proxy.global_frame_routing_id());
    CHECK_EQ(erased_frames, 1u);
    if (main_rfh_id_iter->second.empty()) {
      main_rfh_ids_by_page_context_.erase(main_rfh_id_iter);
    }
  }
  const size_t erased_contexts =
      page_contexts_by_rfh_id_.erase(rfh_proxy.global_frame_routing_id());
  CHECK_EQ(erased_contexts, 1u);
}

void UIThreadStorage::OnCurrentMainFrameChanged(
    const PageContext& page_context,
    const RenderFrameHostProxy& rfh_proxy,
    bool is_current) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Find the map containing all main frames for this context.
  const auto main_frames_iter =
      main_rfh_ids_by_page_context_.find(page_context);
  CHECK(main_frames_iter != main_rfh_ids_by_page_context_.end());
  // Find this specific frame.
  const auto rfh_id_iter =
      main_frames_iter->second.find(rfh_proxy.global_frame_routing_id());
  CHECK(rfh_id_iter != main_frames_iter->second.end());
  CHECK_NE(rfh_id_iter->second, is_current);
  rfh_id_iter->second = is_current;
}

void UIThreadStorage::OnPageNodeAdded(const PageContext& page_context,
                                      WebContentsProxy proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check OnFrameNodeAdded() hasn't recorded `page_context`. If it has, PM is
  // sending FrameNode notifications before the containing PageNode
  // notifications.
  CHECK(!base::Contains(main_rfh_ids_by_page_context_, page_context));
  const auto [_, inserted] =
      web_contents_by_page_context_.emplace(page_context, proxy);
  CHECK(inserted);
}

void UIThreadStorage::OnPageNodeRemoved(const PageContext& page_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check that PM reported all FrameNode's removed before the containing
  // PageNode.
  CHECK(!base::Contains(main_rfh_ids_by_page_context_, page_context));
  const size_t erased = web_contents_by_page_context_.erase(page_context);
  CHECK_EQ(erased, 1u);
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
absl::optional<PageContext> ResourceContextRegistryStorage::PageContextForId(
    const content::GlobalRenderFrameHostId& id) {
  if (static_ui_thread_storage_) {
    return static_ui_thread_storage_->GetPageContextForId(id);
  }
  return absl::nullopt;
}

// static
content::WebContents* ResourceContextRegistryStorage::WebContentsFromContext(
    const PageContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->GetWebContentsFromContext(context)
             : nullptr;
}

// static
content::RenderFrameHost*
ResourceContextRegistryStorage::CurrentMainRenderFrameHostFromContext(
    const PageContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_
                   ->GetCurrentMainRenderFrameHostFromContext(context)
             : nullptr;
}

// static
std::set<content::RenderFrameHost*>
ResourceContextRegistryStorage::AllMainRenderFrameHostsFromContext(
    const PageContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->GetAllMainRenderFrameHostsFromContext(
                   context)
             : std::set<content::RenderFrameHost*>{};
}

const PageNode* ResourceContextRegistryStorage::GetPageNodeForContext(
    const PageContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetWeakNodeForContext(context, page_nodes_by_context_);
}

void ResourceContextRegistryStorage::OnFrameNodeAdded(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  // Unretained is safe because `ui_thread_storage_` is passed to the UI
  // thread to delete.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&UIThreadStorage::OnFrameNodeAdded,
                     base::Unretained(ui_thread_storage_.get()),
                     frame_node->GetPageNode()->GetResourceContext(),
                     frame_node->GetRenderFrameHostProxy(),
                     frame_node->IsMainFrame(), frame_node->IsCurrent()));
}

void ResourceContextRegistryStorage::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  // Unretained is safe because `ui_thread_storage_` is passed to the UI
  // thread to delete.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnFrameNodeRemoved,
                                base::Unretained(ui_thread_storage_.get()),
                                frame_node->GetPageNode()->GetResourceContext(),
                                frame_node->GetRenderFrameHostProxy(),
                                frame_node->IsMainFrame()));
  // Leave the WeakPtr to `frame_node` in `frame_nodes_by_context_` so it can
  // still be resolved until all OnBeforeFrameNodeRemoved() notifications are
  // done. At that point the WeakPtr will be invalidated.
}

void ResourceContextRegistryStorage::OnIsCurrentChanged(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  if (frame_node->IsMainFrame()) {
    // Unretained is safe because `ui_thread_storage_` is deleted on the UI
    // thread.
    content::GetUIThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&UIThreadStorage::OnCurrentMainFrameChanged,
                       base::Unretained(ui_thread_storage_.get()),
                       frame_node->GetPageNode()->GetResourceContext(),
                       frame_node->GetRenderFrameHostProxy(),
                       frame_node->IsCurrent()));
  }
}

void ResourceContextRegistryStorage::OnPageNodeAdded(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto [_, inserted] = page_nodes_by_context_.emplace(
      page_node->GetResourceContext(),
      PageNodeImpl::FromNode(page_node)->GetWeakPtr());
  CHECK(inserted);
  CHECK(ui_thread_storage_);
  // Unretained is safe because `ui_thread_storage_` is deleted on the UI
  // thread.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnPageNodeAdded,
                                base::Unretained(ui_thread_storage_.get()),
                                page_node->GetResourceContext(),
                                page_node->GetContentsProxy()));
}

void ResourceContextRegistryStorage::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  // Unretained is safe because `ui_thread_storage_` is deleted on the UI
  // thread.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnPageNodeRemoved,
                                base::Unretained(ui_thread_storage_.get()),
                                page_node->GetResourceContext()));
  // Leave the WeakPtr to `page_node` in `page_nodes_by_context_` so it can
  // still be resolved until all OnBeforePageNodeRemoved() notifications are
  // done. At that point the WeakPtr will be invalidated.
}

void ResourceContextRegistryStorage::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RegisterObject(&page_registry_);
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
}

void ResourceContextRegistryStorage::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->UnregisterObject(&page_registry_);
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
