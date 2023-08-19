// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_tab_helper.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/performance_manager_registry_impl.h"
#include "components/performance_manager/render_process_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {

namespace {

// Returns true if the opener relationship exists, false otherwise.
bool ConnectWindowOpenRelationshipIfExists(PerformanceManagerTabHelper* helper,
                                           content::WebContents* web_contents) {
  // Prefer to use GetOpener() if available, as it is more specific and points
  // directly to the frame that actually called window.open.
  auto* opener_rfh = web_contents->GetOpener();
  if (!opener_rfh) {
    // If the child page is opened with "noopener" then the parent document
    // maintains the ability to close the child, but the child can't reach back
    // and see it's parent. In this case there will be no "opener", but there
    // will be an "original opener".
    if (content::WebContents* original_opener_wc =
            web_contents->GetFirstWebContentsInLiveOriginalOpenerChain()) {
      opener_rfh = original_opener_wc->GetPrimaryMainFrame();
    }
  }

  if (!opener_rfh)
    return false;

  // You can't simultaneously be a portal (an embedded child element of a
  // document loaded via the <portal> tag) and a popup (a child document
  // loaded in a new window).
  DCHECK(!web_contents->IsPortal());

  // Connect this new page to its opener.
  auto* opener_wc = content::WebContents::FromRenderFrameHost(opener_rfh);
  auto* opener_helper = PerformanceManagerTabHelper::FromWebContents(opener_wc);
  DCHECK(opener_helper);  // We should already have seen the opener WC.

  // On CrOS the opener can be the ChromeKeyboardWebContents, whose RFHs never
  // make it to a "created" state, so the PM never learns about them.
  // https://crbug.com/1090374
  auto* opener_frame_node = opener_helper->GetFrameNode(opener_rfh);
  if (!opener_frame_node)
    return false;

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetOpenerFrameNode,
                                base::Unretained(helper->primary_page_node()),
                                base::Unretained(opener_frame_node)));
  return true;
}

}  // namespace

PerformanceManagerTabHelper::PageData::PageData() = default;
PerformanceManagerTabHelper::PageData::~PageData() = default;

PerformanceManagerTabHelper::PerformanceManagerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PerformanceManagerTabHelper>(*web_contents) {
  // We have an early WebContents creation hook so should see it when there is
  // only a single frame, and it is not yet created. We sanity check that here.
#if DCHECK_IS_ON()
  DCHECK(!web_contents->GetPrimaryMainFrame()->IsRenderFrameLive());
  size_t frame_count = 0;
  web_contents->ForEachRenderFrameHost(
      [&frame_count](content::RenderFrameHost* render_frame_host) {
        ++frame_count;
      });
  DCHECK_EQ(1u, frame_count);
#endif

  // Create the page node.
  std::unique_ptr<PageData> page = std::make_unique<PageData>();
  page->page_node = PerformanceManagerImpl::CreatePageNode(
      WebContentsProxy(weak_factory_.GetWeakPtr()),
      web_contents->GetBrowserContext()->UniqueId(),
      web_contents->GetVisibleURL(),
      web_contents->GetVisibility() == content::Visibility::VISIBLE,
      web_contents->IsCurrentlyAudible(), web_contents->GetLastActiveTime(),
      // TODO(crbug.com/1211368): Support MPArch fully!
      PageNode::PageState::kActive);
  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  DCHECK(main_rfh);
  page->main_frame_tree_node_id = main_rfh->GetFrameTreeNodeId();
  primary_page_ = page.get();
  auto result = pages_.insert(std::move(page));
  DCHECK(result.second);

  ConnectWindowOpenRelationshipIfExists(this, web_contents);
}

PerformanceManagerTabHelper::~PerformanceManagerTabHelper() {
  DCHECK(pages_.empty());
  DCHECK(!primary_page_);
  DCHECK(frames_.empty());
}

void PerformanceManagerTabHelper::TearDown() {
  // Ship our page and frame nodes to the PerformanceManagerImpl for
  // incineration.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  for (auto& kv : frames_) {
    std::unique_ptr<FrameNodeImpl> frame_node = std::move(kv.second);

    // Notify observers.
    for (Observer& observer : observers_)
      observer.OnBeforeFrameNodeRemoved(this, frame_node.get());

    // Ensure the node will be deleted on the graph sequence.
    nodes.push_back(std::move(frame_node));
  }
  for (auto& kv : pages_) {
    std::unique_ptr<PageNodeImpl> page_node = std::move(kv->page_node);
    nodes.push_back(std::move(page_node));
  }

  // primary_page ptr should be cleared before pages_ is cleared, otherwise
  // it becomes dangling.
  primary_page_ = nullptr;
  pages_.clear();
  frames_.clear();

  // Delete the page and its entire frame tree from the graph.
  PerformanceManagerImpl::BatchDeleteNodes(std::move(nodes));

  if (destruction_observer_) {
    destruction_observer_->OnPerformanceManagerTabHelperDestroying(
        web_contents());
  }

  // Unsubscribe from the associated WebContents.
  Observe(nullptr);
}

PageNodeImpl* PerformanceManagerTabHelper::GetPageNodeForRenderFrameHost(
    content::RenderFrameHost* rfh) {
  DCHECK_NE(nullptr, rfh);
  // TODO(crbug.com/1211368): Make this lookup the appropriate PageNode once
  // MPArch support is completed. For now, everything is artifically descended
  // from the primary page node. Add tests for this function at that point.
  auto* wc = content::WebContents::FromRenderFrameHost(rfh);
  if (wc != web_contents())
    return nullptr;
  return primary_page_node();
}

void PerformanceManagerTabHelper::SetDestructionObserver(
    DestructionObserver* destruction_observer) {
  DCHECK(!destruction_observer || !destruction_observer_);
  destruction_observer_ = destruction_observer;
}

void PerformanceManagerTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_NE(nullptr, render_frame_host);
  // This must not exist in the map yet.
  DCHECK(!base::Contains(frames_, render_frame_host));

  content::RenderFrameHost* parent = render_frame_host->GetParent();
  FrameNodeImpl* parent_frame_node = nullptr;
  if (parent) {
    DCHECK(base::Contains(frames_, parent));
    parent_frame_node = frames_[parent].get();
  }

  // Ideally, creation would not be required here, but it is possible in tests
  // for the RenderProcessUserData to not have attached at this point.
  PerformanceManagerRegistryImpl::GetInstance()
      ->EnsureProcessNodeForRenderProcessHost(render_frame_host->GetProcess());

  auto* process_node = RenderProcessUserData::GetForRenderProcessHost(
                           render_frame_host->GetProcess())
                           ->process_node();

  auto* site_instance = render_frame_host->GetSiteInstance();

  // Create the frame node, and provide a callback that will run in the graph to
  // initialize it.
  // TODO(crbug.com/1211368): Actually look up the appropriate page to wire this
  // frame up to!
  std::unique_ptr<FrameNodeImpl> frame =
      PerformanceManagerImpl::CreateFrameNode(
          process_node, primary_page_node(), parent_frame_node,
          render_frame_host->GetRoutingID(),
          blink::LocalFrameToken(render_frame_host->GetFrameToken()),
          site_instance->GetBrowsingInstanceId(), site_instance->GetId(),
          base::BindOnce(
              [](const GURL& url, bool is_current, FrameNodeImpl* frame_node) {
                if (!url.is_empty())
                  frame_node->OnNavigationCommitted(url,
                                                    /* same_document */ false);
                frame_node->SetIsCurrent(is_current);
              },
              render_frame_host->GetLastCommittedURL(),
              render_frame_host->IsActive()));

  frames_[render_frame_host] = std::move(frame);
}

void PerformanceManagerTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = frames_.find(render_frame_host);
  DCHECK(it != frames_.end());

  std::unique_ptr<FrameNodeImpl> frame_node = std::move(it->second);

  // Notify observers.
  for (Observer& observer : observers_)
    observer.OnBeforeFrameNodeRemoved(this, frame_node.get());

  // Then delete the node.
  PerformanceManagerImpl::DeleteNode(std::move(frame_node));
  frames_.erase(it);
}

void PerformanceManagerTabHelper::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // |old_host| is null when a new frame tree position is being created and a
  // new frame is its first occupant.
  FrameNodeImpl* old_frame = nullptr;
  if (old_host) {
    auto it = frames_.find(old_host);
    if (it != frames_.end()) {
      // This can be received for a frame that hasn't yet been created. We can
      // safely ignore this. It would be nice to track those frames too, but
      // since they're not yet "created" we'd have no guarantee of seeing a
      // corresponding delete and the frames can be leaked.
      old_frame = it->second.get();
    }
  }

  // It's entirely possible that this is the first time we're seeing this frame.
  // We'll eventually see a corresponding RenderFrameCreated if the frame ends
  // up actually being needed, so we can ignore it until that time. Artificially
  // creating the frame causes problems because we aren't guaranteed to see a
  // subsequent RenderFrameCreated call, meaning we won't see a
  // RenderFrameDeleted, and the frame node will be leaked until process tear
  // down.
  DCHECK(new_host);
  FrameNodeImpl* new_frame = nullptr;
  auto it = frames_.find(new_host);
  if (it != frames_.end()) {
    new_frame = it->second.get();
  } else {
    DCHECK(!new_host->IsRenderFrameLive())
        << "There shouldn't be a case where RenderFrameHostChanged is "
           "dispatched before RenderFrameCreated with a live RenderFrame\n";
  }
  // If neither frame could be looked up there's nothing to do.
  if (!old_frame && !new_frame)
    return;

  // Perform the swap in the graph.
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(
                     [](FrameNodeImpl* old_frame, FrameNodeImpl* new_frame) {
                       if (old_frame) {
                         // Prerendering is a special case where
                         // old_frame->is_current() may be false.
                         // TODO(https://crbug.com/1211368): assert that
                         // old_frame->is_current() or its PageState is
                         // kPrerendering.
                         old_frame->SetIsCurrent(false);
                       }

                       if (new_frame) {
                         // The very first frame to be created is already
                         // current by default except in the special case of
                         // prerendering.
                         // TODO(https://crbug.com/1211368): assert that
                         // old_frame is null or its PageState is kPrerendering.
                         new_frame->SetIsCurrent(true);
                       }
                     },
                     old_frame, new_frame));
}

void PerformanceManagerTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  const bool is_visible = visibility == content::Visibility::VISIBLE;
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::SetIsVisible,
                     base::Unretained(primary_page_node()), is_visible));
}

void PerformanceManagerTabHelper::OnAudioStateChanged(bool audible) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::SetIsAudible,
                     base::Unretained(primary_page_node()), audible));
}

void PerformanceManagerTabHelper::OnFrameAudioStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_audible) {
  auto frame_it = frames_.find(render_frame_host);
  // Ideally this would be a DCHECK, but RenderFrameHost sends out one last
  // notification in its destructor; at this point we've already torn down the
  // FrameNode in response to the RenderFrameDeleted which comes *before* the
  // destructor is run.
  if (frame_it == frames_.end()) {
    // We should only ever see this for a frame transitioning to *not* audible.
    DCHECK(!is_audible);
    return;
  }
  auto* frame_node = frame_it->second.get();
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&FrameNodeImpl::SetIsAudible,
                                base::Unretained(frame_node), is_audible));
}

void PerformanceManagerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // Grab the current time up front, as this is as close as we'll get to the
  // original commit time.
  base::TimeTicks navigation_committed_time = base::TimeTicks::Now();

  // Find the associated frame node.
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  auto frame_it = frames_.find(render_frame_host);
  // TODO(siggi): Ideally this would be a DCHECK, but it seems it's possible
  //     to get a DidFinishNavigation notification for a deleted frame with
  //     the network service.
  if (frame_it == frames_.end())
    return;
  auto* frame_node = frame_it->second.get();

  // Notify the frame of the committed URL.
  GURL url = navigation_handle->GetURL();
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&FrameNodeImpl::OnNavigationCommitted,
                                base::Unretained(frame_node), url,
                                navigation_handle->IsSameDocument()));

  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // Make sure the hierarchical structure is constructed before sending signal
  // to the performance manager.
  OnMainFrameNavigation(navigation_handle->GetNavigationId(),
                        navigation_handle->IsSameDocument());
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          &PageNodeImpl::OnMainFrameNavigationCommitted,
          base::Unretained(primary_page_node()),
          navigation_handle->IsSameDocument(), navigation_committed_time,
          navigation_handle->GetNavigationId(), url,
          navigation_handle->GetWebContents()->GetContentsMimeType()));
}

void PerformanceManagerTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  DCHECK(primary_page_);

  // TODO(crbug.com/1418410): This logic belongs in the policy layer rather than
  // here. If a page has no <title> element on first load, the first change of
  // title will be ignored no matter much later it happens.
  if (!primary_page_->first_time_title_set) {
    primary_page_->first_time_title_set = true;
    return;
  }
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::OnTitleUpdated,
                                base::Unretained(primary_page_node())));
}

void PerformanceManagerTabHelper::InnerWebContentsAttached(
    content::WebContents* inner_web_contents,
    content::RenderFrameHost* render_frame_host,
    bool /* is_full_page */) {
  // Note that we sometimes learn of contents creation at this point (before
  // other helpers get a chance to attach), so we need to ensure our helper
  // exists.
  CreateForWebContents(inner_web_contents);
  auto* helper = FromWebContents(inner_web_contents);
  DCHECK(helper);
  auto* page = helper->primary_page_node();
  DCHECK(page);
  auto* frame = GetFrameNode(render_frame_host);

  // Determine the embedded type.
  auto embedding_type = PageNode::EmbeddingType::kInvalid;
  if (inner_web_contents->IsPortal()) {
    embedding_type = PageNode::EmbeddingType::kPortal;

    // In the case of portals there can be a temporary RFH that is created that
    // will never actually be committed to the frame tree (for which we'll never
    // see RenderFrameCreated and RenderFrameDestroyed notifications). Find a
    // parent that we do know about instead. Note that this is not *always*
    // true, because portals are reusable.
    if (!frame)
      frame = GetFrameNode(render_frame_host->GetParent());
  } else {
    embedding_type = PageNode::EmbeddingType::kGuestView;
    // For a guest view, the RFH should already have been seen.
    // Note that guest views can simultaneously have openers *and* be embedded.
  }
  DCHECK_NE(PageNode::EmbeddingType::kInvalid, embedding_type);
  DCHECK(frame);

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::SetEmbedderFrameNodeAndEmbeddingType,
                     base::Unretained(page), base::Unretained(frame),
                     embedding_type));
}

void PerformanceManagerTabHelper::InnerWebContentsDetached(
    content::WebContents* inner_web_contents) {
  auto* helper = FromWebContents(inner_web_contents);
  DCHECK(helper);
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::ClearEmbedderFrameNodeAndEmbeddingType,
                     base::Unretained(helper->primary_page_node())));
}

void PerformanceManagerTabHelper::WebContentsDestroyed() {
  // Remember the contents, as TearDown clears observer.
  auto* contents = web_contents();
  TearDown();
  // Immediately remove ourselves from the WCUD. After TearDown the tab helper
  // is in an inconsistent state. This will prevent other
  // WCO::WebContentsDestroyed handlers from trying to access the tab helper in
  // this inconsistent state.
  contents->RemoveUserData(UserDataKey());
}

void PerformanceManagerTabHelper::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  DCHECK(primary_page_);

  // This favicon change might have been initiated by a different frame some
  // time ago and the main frame might have changed.
  if (!render_frame_host->IsActive())
    return;

  // TODO(crbug.com/1418410): This logic belongs in the policy layer rather than
  // here. If a page has no favicon on first load, the first change of favicon
  // will be ignored no matter much later it happens.
  if (!primary_page_->first_time_favicon_set) {
    primary_page_->first_time_favicon_set = true;
    return;
  }
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::OnFaviconUpdated,
                                base::Unretained(primary_page_node())));
}

void PerformanceManagerTabHelper::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetIsFocused,
                                base::Unretained(primary_page_node()),
                                /*is_focused=*/true));
}

void PerformanceManagerTabHelper::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetIsFocused,
                                base::Unretained(primary_page_node()),
                                /*is_focused=*/false));
}

void PerformanceManagerTabHelper::AboutToBeDiscarded(
    content::WebContents* new_contents) {
  DCHECK(primary_page_);

  base::WeakPtr<PageNode> new_page_node =
      PerformanceManager::GetPrimaryPageNodeForWebContents(new_contents);

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::OnAboutToBeDiscarded,
                     base::Unretained(primary_page_node()), new_page_node));
}

void PerformanceManagerTabHelper::BindDocumentCoordinationUnit(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver) {
  auto it = frames_.find(render_frame_host);
  DCHECK(it != frames_.end());

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&FrameNodeImpl::Bind, base::Unretained(it->second.get()),
                     std::move(receiver)));
}

content::WebContents* PerformanceManagerTabHelper::GetWebContents() const {
  return web_contents();
}

int64_t PerformanceManagerTabHelper::LastNavigationId() const {
  DCHECK(primary_page_);
  return primary_page_->last_navigation_id;
}

int64_t PerformanceManagerTabHelper::LastNewDocNavigationId() const {
  DCHECK(primary_page_);
  return primary_page_->last_new_doc_navigation_id;
}

FrameNodeImpl* PerformanceManagerTabHelper::GetFrameNode(
    content::RenderFrameHost* render_frame_host) {
  auto it = frames_.find(render_frame_host);
  return it != frames_.end() ? it->second.get() : nullptr;
}

void PerformanceManagerTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PerformanceManagerTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PerformanceManagerTabHelper::OnMainFrameNavigation(int64_t navigation_id,
                                                        bool same_doc) {
  DCHECK(primary_page_);

  primary_page_->last_navigation_id = navigation_id;
  if (!same_doc)
    primary_page_->last_new_doc_navigation_id = navigation_id;
  primary_page_->ukm_source_id =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetUkmSourceId,
                                base::Unretained(primary_page_node()),
                                primary_page_->ukm_source_id));

  primary_page_->first_time_title_set = false;
  primary_page_->first_time_favicon_set = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceManagerTabHelper);

}  // namespace performance_manager
