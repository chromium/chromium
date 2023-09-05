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
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/browser_child_process_host_proxy.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

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

  // FrameContext accessors.
  content::RenderFrameHost* GetRenderFrameHostFromContext(
      const FrameContext& context) const;

  // PageContext accessors.
  absl::optional<PageContext> GetPageContextForId(
      const content::GlobalRenderFrameHostId& id) const;
  content::WebContents* GetWebContentsFromContext(
      const PageContext& context) const;
  content::RenderFrameHost* GetCurrentMainRenderFrameHostFromContext(
      const PageContext& context) const;
  std::set<content::RenderFrameHost*> GetAllMainRenderFrameHostsFromContext(
      const PageContext& context) const;

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

  // WorkerContext accessors.
  bool IsRegisteredWorkerContext(const WorkerContext& context) const;

  // Update storage based on changes in the PM graph.

  // Called when the FrameNode with context `frame_context`, belonging to the
  // page `page_context`, is added to the PM graph.
  void OnFrameNodeAdded(const FrameContext& frame_context,
                        const PageContext& page_context,
                        const RenderFrameHostProxy& rfh_proxy,
                        bool is_main_frame,
                        bool is_current);

  // Called when the FrameNode with context `frame_context`, belonging to the
  // page `page_context`, is removed from the PM graph.
  void OnFrameNodeRemoved(const FrameContext& frame_context,
                          const PageContext& page_context,
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

  void OnWorkerNodeAdded(const WorkerContext& worker_context);

  void OnWorkerNodeRemoved(const WorkerContext& worker_context);

 private:
  // Asserts that `context` isn't in any map.
  void CheckProcessContextUnregistered(const ProcessContext& context) const;

  // FrameContext storage

  std::map<FrameContext, content::GlobalRenderFrameHostId>
      rfh_ids_by_frame_context_;

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

  // ProcessContext storage

  absl::optional<ProcessContext> browser_process_context_;
  std::map<RenderProcessHostId, ProcessContext> process_contexts_by_rph_id_;
  std::map<BrowserChildProcessHostId, ProcessContext>
      process_contexts_by_bcph_id_;
  std::map<ProcessContext, RenderProcessHostId> rph_ids_by_process_context_;
  std::map<ProcessContext, BrowserChildProcessHostId>
      bcph_ids_by_process_context_;

  // WorkerContext storage

  // All contexts known to the registry. Prevents the registry from converting a
  // randomly-generated blink::WorkerToken that doesn't correspond to a real
  // worker into a WorkerContext.
  std::set<WorkerContext> worker_contexts_;
};

content::RenderFrameHost* UIThreadStorage::GetRenderFrameHostFromContext(
    const FrameContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto it = rfh_ids_by_frame_context_.find(context);
  return it == rfh_ids_by_frame_context_.end()
             ? nullptr
             : content::RenderFrameHost::FromID(it->second);
}

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

bool UIThreadStorage::IsRegisteredWorkerContext(
    const WorkerContext& context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::Contains(worker_contexts_, context);
}

void UIThreadStorage::OnFrameNodeAdded(const FrameContext& frame_context,
                                       const PageContext& page_context,
                                       const RenderFrameHostProxy& rfh_proxy,
                                       bool is_main_frame,
                                       bool is_current) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto [_, inserted] = rfh_ids_by_frame_context_.emplace(
      frame_context, rfh_proxy.global_frame_routing_id());
  CHECK(inserted);

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

void UIThreadStorage::OnFrameNodeRemoved(const FrameContext& frame_context,
                                         const PageContext& page_context,
                                         const RenderFrameHostProxy& rfh_proxy,
                                         bool is_main_frame) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const size_t erased = rfh_ids_by_frame_context_.erase(frame_context);
  CHECK_EQ(erased, 1u);

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

void UIThreadStorage::OnWorkerNodeAdded(const WorkerContext& worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const auto [_, inserted] = worker_contexts_.insert(worker_context);
  CHECK(inserted);
}

void UIThreadStorage::OnWorkerNodeRemoved(const WorkerContext& worker_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const size_t erased = worker_contexts_.erase(worker_context);
  CHECK_EQ(erased, 1u);
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
absl::optional<FrameContext>
ResourceContextRegistryStorage::FrameContextForRenderFrameHost(
    content::RenderFrameHost* host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (static_ui_thread_storage_ && host) {
    // Re-use the LocalFrameToken as a ResourceContext token. There's no need
    // to check if the token is in storage since `host` is a live frame.
    return FrameContext(host->GetFrameToken());
  }
  return absl::nullopt;
}

// static
content::RenderFrameHost*
ResourceContextRegistryStorage::RenderFrameHostFromContext(
    const FrameContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The ResourceContext token is a converted LocalFrameToken, but
  // RenderFrameHost::FromToken() also needs a process ID, so a map from
  // context->RenderFrameHost needs to be stored in the registry.
  return static_ui_thread_storage_
             ? static_ui_thread_storage_->GetRenderFrameHostFromContext(context)
             : nullptr;
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

// static
absl::optional<WorkerContext>
ResourceContextRegistryStorage::WorkerContextForWorkerToken(
    const blink::WorkerToken& token) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Re-use the WorkerToken as a ResourceContext token.
  const resource_attribution::WorkerContext context(token);
  if (static_ui_thread_storage_ &&
      static_ui_thread_storage_->IsRegisteredWorkerContext(context)) {
    return context;
  }
  return absl::nullopt;
}

// static
absl::optional<blink::WorkerToken>
ResourceContextRegistryStorage::WorkerTokenFromContext(
    const WorkerContext& context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (static_ui_thread_storage_ &&
      static_ui_thread_storage_->IsRegisteredWorkerContext(context)) {
    // The ResourceContext token is a converted WorkerToken.
    return blink::WorkerToken(context.value());
  }
  return absl::nullopt;
}

const FrameNode* ResourceContextRegistryStorage::GetFrameNodeForContext(
    const FrameContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = frame_nodes_by_context_.find(context);
  return it != frame_nodes_by_context_.end() ? it->second : nullptr;
}

const PageNode* ResourceContextRegistryStorage::GetPageNodeForContext(
    const PageContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = page_nodes_by_context_.find(context);
  return it != page_nodes_by_context_.end() ? it->second : nullptr;
}

const ProcessNode* ResourceContextRegistryStorage::GetProcessNodeForContext(
    const ProcessContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = process_nodes_by_context_.find(context);
  return it != process_nodes_by_context_.end() ? it->second : nullptr;
}

const WorkerNode* ResourceContextRegistryStorage::GetWorkerNodeForContext(
    const WorkerContext& context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto it = worker_nodes_by_context_.find(context);
  return it != worker_nodes_by_context_.end() ? it->second : nullptr;
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
                     frame_node->GetResourceContext(),
                     frame_node->GetPageNode()->GetResourceContext(),
                     frame_node->GetRenderFrameHostProxy(),
                     frame_node->IsMainFrame(), frame_node->IsCurrent()));
  const auto [_, inserted] = frame_nodes_by_context_.emplace(
      frame_node->GetResourceContext(), frame_node);
  CHECK(inserted);
}

void ResourceContextRegistryStorage::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t erased =
      frame_nodes_by_context_.erase(frame_node->GetResourceContext());
  CHECK_EQ(erased, 1u);
  CHECK(ui_thread_storage_);
  // Unretained is safe because `ui_thread_storage_` is passed to the UI
  // thread to delete.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnFrameNodeRemoved,
                                base::Unretained(ui_thread_storage_.get()),
                                frame_node->GetResourceContext(),
                                frame_node->GetPageNode()->GetResourceContext(),
                                frame_node->GetRenderFrameHostProxy(),
                                frame_node->IsMainFrame()));
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
      page_node->GetResourceContext(), page_node);
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
  const size_t erased =
      page_nodes_by_context_.erase(page_node->GetResourceContext());
  CHECK_EQ(erased, 1u);
  CHECK(ui_thread_storage_);
  // Unretained is safe because `ui_thread_storage_` is deleted on the UI
  // thread.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnPageNodeRemoved,
                                base::Unretained(ui_thread_storage_.get()),
                                page_node->GetResourceContext()));
}

void ResourceContextRegistryStorage::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(ui_thread_storage_);
  ProcessContext process_context = process_node->GetResourceContext();
  // Unretained is safe because `ui_thread_storage_` is passed to the UI
  // thread to delete.
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
  // Unretained is safe because `ui_thread_storage_` is passed to the UI
  // thread to delete.
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

void ResourceContextRegistryStorage::OnWorkerNodeAdded(
    const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto [_, inserted] = worker_nodes_by_context_.emplace(
      worker_node->GetResourceContext(), worker_node);
  CHECK(inserted);
  // Unretained is safe because the pointer is deleted on the UI thread.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnWorkerNodeAdded,
                                base::Unretained(ui_thread_storage_.get()),
                                worker_node->GetResourceContext()));
}

void ResourceContextRegistryStorage::OnBeforeWorkerNodeRemoved(
    const WorkerNode* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t erased =
      worker_nodes_by_context_.erase(worker_node->GetResourceContext());
  CHECK_EQ(erased, 1u);
  // Unretained is safe because the pointer is deleted on the UI thread.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&UIThreadStorage::OnWorkerNodeRemoved,
                                base::Unretained(ui_thread_storage_.get()),
                                worker_node->GetResourceContext()));
}

void ResourceContextRegistryStorage::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RegisterObject(&frame_registry_);
  graph->RegisterObject(&page_registry_);
  graph->RegisterObject(&process_registry_);
  graph->RegisterObject(&worker_registry_);
  graph->AddFrameNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
  graph->AddWorkerNodeObserver(this);
}

void ResourceContextRegistryStorage::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveFrameNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
  graph->RemoveWorkerNodeObserver(this);
  graph->UnregisterObject(&frame_registry_);
  graph->UnregisterObject(&page_registry_);
  graph->UnregisterObject(&process_registry_);
  graph->UnregisterObject(&worker_registry_);
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
