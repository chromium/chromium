// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_tab_helper.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
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
    // and see it's parent. In this case there will be no "Opener", but there
    // will be an "OriginalOpener".
    opener_rfh = web_contents->GetOriginalOpener();
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
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetOpenerFrameNodeAndOpenedType,
                                base::Unretained(helper->page_node()),
                                base::Unretained(opener_frame_node),
                                PageNode::OpenedType::kPopup));
  return true;
}

}  // namespace

PerformanceManagerTabHelper::PerformanceManagerTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // Create the page node.
  page_node_ = PerformanceManagerImpl::CreatePageNode(
      WebContentsProxy(weak_factory_.GetWeakPtr()),
      web_contents->GetBrowserContext()->UniqueId(),
      web_contents->GetVisibleURL(),
      web_contents->GetVisibility() == content::Visibility::VISIBLE,
      web_contents->IsCurrentlyAudible(), web_contents->GetLastActiveTime());

  // We have an early WebContents creation hook so should see it when there is
  // only a single frame, and it is not yet created. We sanity check that here.
#if DCHECK_IS_ON()
  DCHECK(!web_contents->GetMainFrame()->IsRenderFrameCreated());
  std::vector<content::RenderFrameHost*> frames = web_contents->GetAllFrames();
  DCHECK_EQ(1u, frames.size());
  DCHECK_EQ(web_contents->GetMainFrame(), frames[0]);
#endif

  ConnectWindowOpenRelationshipIfExists(this, web_contents);
}

PerformanceManagerTabHelper::~PerformanceManagerTabHelper() {
  DCHECK(!page_node_);
  DCHECK(frames_.empty());
}

void PerformanceManagerTabHelper::TearDown() {
  // Ship our page and frame nodes to the PerformanceManagerImpl for
  // incineration.
  std::vector<std::unique_ptr<NodeBase>> nodes;
  nodes.push_back(std::move(page_node_));
  for (auto& kv : frames_) {
    std::unique_ptr<FrameNodeImpl> frame_node = std::move(kv.second);

    // Notify observers.
    for (Observer& observer : observers_)
      observer.OnBeforeFrameNodeRemoved(this, frame_node.get());

    // Ensure the node will be deleted on the graph sequence.
    nodes.push_back(std::move(frame_node));
  }

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
  std::unique_ptr<FrameNodeImpl> frame =
      PerformanceManagerImpl::CreateFrameNode(
          process_node, page_node_.get(), parent_frame_node,
          render_frame_host->GetFrameTreeNodeId(),
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
              render_frame_host->IsCurrent()));

  frames_[render_frame_host] = std::move(frame);
}

void PerformanceManagerTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = frames_.find(render_frame_host);
  if (it == frames_.end()) {
    // https://crbug.com/948088.
    // At the present time (May 2019), it's possible for speculative render
    // frame hosts to exist at the time this TabHelper is attached to a
    // WebContents. These speculative render frame hosts are not exposed in
    // enumeration, and so may be first observed at deletion time.
    return;
  }
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
  } else if (new_host->IsRenderFrameCreated()) {
    // https://crbug.com/948088.
    // In the case of speculative frames already existent and created at attach
    // time, fake the creation event at this point.
    RenderFrameCreated(new_host);

    new_frame = frames_[new_host].get();
    DCHECK_NE(nullptr, new_frame);
  }
  // If neither frame could be looked up there's nothing to do.
  if (!old_frame && !new_frame)
    return;

  // Perform the swap in the graph.
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(
                     [](FrameNodeImpl* old_frame, FrameNodeImpl* new_frame) {
                       if (old_frame) {
                         DCHECK(old_frame->is_current());
                         old_frame->SetIsCurrent(false);
                       }
                       if (new_frame) {
                         if (!new_frame->is_current()) {
                           new_frame->SetIsCurrent(true);
                         } else {
                           // The very first frame to be created is already
                           // current by default. In which case the swap must be
                           // from no frame to a frame.
                           DCHECK(!old_frame);
                         }
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
                     base::Unretained(page_node_.get()), is_visible));
}

void PerformanceManagerTabHelper::OnAudioStateChanged(bool audible) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetIsAudible,
                                base::Unretained(page_node_.get()), audible));
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

  if (!navigation_handle->IsInMainFrame())
    return;

  // Make sure the hierarchical structure is constructed before sending signal
  // to the performance manager.
  OnMainFrameNavigation(navigation_handle->GetNavigationId(),
                        navigation_handle->IsSameDocument());
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(
          &PageNodeImpl::OnMainFrameNavigationCommitted,
          base::Unretained(page_node_.get()),
          navigation_handle->IsSameDocument(), navigation_committed_time,
          navigation_handle->GetNavigationId(), url,
          navigation_handle->GetWebContents()->GetContentsMimeType()));
}

void PerformanceManagerTabHelper::TitleWasSet(content::NavigationEntry* entry) {
  // TODO(siggi): This logic belongs in the policy layer rather than here.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::OnTitleUpdated,
                                base::Unretained(page_node_.get())));
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
  auto* page = helper->page_node_.get();
  DCHECK(page);
  auto* frame = GetFrameNode(render_frame_host);

  // Determine the opened type.
  auto opened_type = PageNode::OpenedType::kInvalid;
  if (inner_web_contents->IsPortal()) {
    opened_type = PageNode::OpenedType::kPortal;

    // In the case of portals there can be a temporary RFH that is created that
    // will never actually be committed to the frame tree (for which we'll never
    // see RenderFrameCreated and RenderFrameDestroyed notifications). Find a
    // parent that we do know about instead. Note that this is not *always*
    // true, because portals are reusable.
    if (!frame)
      frame = GetFrameNode(render_frame_host->GetParent());
  } else {
    opened_type = PageNode::OpenedType::kGuestView;
    // For a guest view, the RFH should already have been seen.

    // Note that guest views can simultaneously have openers *and* be embedded.
    // The embedded relationship has higher priority, but we'll fall back to
    // using the window.open relationship if the embedded relationship is
    // severed.
  }
  DCHECK_NE(PageNode::OpenedType::kInvalid, opened_type);
  if (!frame) {
    DCHECK(!render_frame_host->IsRenderFrameCreated());
    DCHECK(!inner_web_contents->IsPortal());
    // TODO(crbug.com/1133361):
    // WebContentsImplBrowserTest.AttachNestedInnerWebContents calls
    // WebContents::AttachInnerWebContents without creating RenderFrame.
    // Removing this conditional once either the test is fixed or this function
    // is adjusted to handle the case without the render frame.
    return;
  }

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::SetOpenerFrameNodeAndOpenedType,
                                base::Unretained(page), base::Unretained(frame),
                                opened_type));
}

void PerformanceManagerTabHelper::InnerWebContentsDetached(
    content::WebContents* inner_web_contents) {
  auto* helper = FromWebContents(inner_web_contents);
  DCHECK(helper);

  // Fall back to using the window.open opener if it exists. If not, simply
  // clear the opener relationship entirely.
  if (!ConnectWindowOpenRelationshipIfExists(helper, inner_web_contents)) {
    PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE,
        base::BindOnce(&PageNodeImpl::ClearOpenerFrameNodeAndOpenedType,
                       base::Unretained(helper->page_node())));
  }
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
  // This favicon change might have been initiated by a different frame some
  // time ago and the main frame might have changed.
  if (!render_frame_host->IsCurrent())
    return;

  // TODO(siggi): This logic belongs in the policy layer rather than here.
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&PageNodeImpl::OnFaviconUpdated,
                                base::Unretained(page_node_.get())));
}

void PerformanceManagerTabHelper::BindDocumentCoordinationUnit(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver) {
  // TODO(https://crbug.com/987445): Why else than due to speculative render
  //     frame hosts would this happen? Is there a race between the RFH creation
  //     notification and the mojo interface request?
  auto it = frames_.find(render_frame_host);
  if (it == frames_.end()) {
    if (render_frame_host->IsRenderFrameCreated()) {
      // This must be a speculative render frame host, generate a creation event
      // for it a this point
      RenderFrameCreated(render_frame_host);

      it = frames_.find(render_frame_host);
      DCHECK(it != frames_.end());
    } else {
      // It would be nice to know what's up here, maybe there's a race between
      // in-progress interface requests and the frame deletion?
      return;
    }
  }

  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&FrameNodeImpl::Bind, base::Unretained(it->second.get()),
                     std::move(receiver)));
}

content::WebContents* PerformanceManagerTabHelper::GetWebContents() const {
  return web_contents();
}

int64_t PerformanceManagerTabHelper::LastNavigationId() const {
  return last_navigation_id_;
}

int64_t PerformanceManagerTabHelper::LastNewDocNavigationId() const {
  return last_new_doc_navigation_id_;
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
  last_navigation_id_ = navigation_id;
  if (!same_doc)
    last_new_doc_navigation_id_ = navigation_id;
  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE,
      base::BindOnce(&PageNodeImpl::SetUkmSourceId,
                     base::Unretained(page_node_.get()), ukm_source_id_));

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PerformanceManagerTabHelper)

}  // namespace performance_manager
