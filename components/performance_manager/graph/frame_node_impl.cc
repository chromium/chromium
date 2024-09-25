// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/frame_node_impl.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/initializing_frame_node_observer.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/graph/worker_node_impl.h"
#include "components/performance_manager/public/v8_memory/web_memory.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom.h"

namespace performance_manager {

namespace {

bool IsParentIntersectingLargeArea(FrameNodeImpl* frame_node) {
  FrameNodeImpl* parent = frame_node->parent_frame_node();
  if (!parent || parent->process_node() != frame_node->process_node()) {
    // `frame_node is a local root. Assume it is intersecting with a large area
    // of the viewport.
    return true;
  }

  return parent->GetViewportIntersection() &&
         parent->GetViewportIntersection()->is_intersecting_large_area();
}

}  // namespace

// static
constexpr char FrameNodeImpl::kDefaultPriorityReason[] =
    "default frame priority";

using PriorityAndReason = execution_context_priority::PriorityAndReason;

FrameNodeImpl::FrameNodeImpl(
    ProcessNodeImpl* process_node,
    PageNodeImpl* page_node,
    FrameNodeImpl* parent_frame_node,
    FrameNodeImpl* outer_document_for_fenced_frame,
    int render_frame_id,
    const blink::LocalFrameToken& frame_token,
    content::BrowsingInstanceId browsing_instance_id,
    content::SiteInstanceGroupId site_instance_group_id,
    bool is_current)
    : parent_frame_node_(parent_frame_node),
      outer_document_for_fenced_frame_(outer_document_for_fenced_frame),
      page_node_(page_node),
      process_node_(process_node),
      render_frame_id_(render_frame_id),
      frame_token_(frame_token),
      browsing_instance_id_(browsing_instance_id),
      site_instance_group_id_(site_instance_group_id),
      render_frame_host_proxy_(content::GlobalRenderFrameHostId(
          process_node->GetRenderProcessHostId().value(),
          render_frame_id)),
      is_current_(is_current) {
  // Nodes are created on the UI thread, then accessed on the PM sequence.
  // `weak_this_` can be returned from GetWeakPtrOnUIThread() and dereferenced
  // on the PM sequence.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  weak_this_ = weak_factory_.GetWeakPtr();

  DCHECK(process_node);
  DCHECK(page_node);
  // A <fencedframe> has no parent node.
  CHECK(!outer_document_for_fenced_frame || !parent_frame_node_);
}

FrameNodeImpl::~FrameNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_worker_nodes_.empty());
  DCHECK(opened_page_nodes_.empty());
  DCHECK(embedded_page_nodes_.empty());
}

void FrameNodeImpl::Bind(
    mojo::PendingReceiver<mojom::DocumentCoordinationUnit> receiver) {
  // It is possible to receive a mojo::PendingReceiver<DocumentCoordinationUnit>
  // when |receiver_| is already bound in these cases:
  // - Navigation from the initial empty document to the first real document.
  // - Navigation rejected by RenderFrameHostImpl::ValidateDidCommitParams().
  // See discussion:
  // https://chromium-review.googlesource.com/c/chromium/src/+/1572459/6#message-bd31f3e73f96bd9f7721be81ba6ac0076d053147
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void FrameNodeImpl::SetNetworkAlmostIdle() {
  document_.network_almost_idle.SetAndMaybeNotify(this, true);
}

void FrameNodeImpl::SetLifecycleState(mojom::LifecycleState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lifecycle_state_.SetAndMaybeNotify(this, state);
}

void FrameNodeImpl::SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  document_.has_nonempty_beforeunload = has_nonempty_beforeunload;
}

void FrameNodeImpl::SetIsAdFrame(bool is_ad_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_ad_frame_.SetAndMaybeNotify(this, is_ad_frame);
}

void FrameNodeImpl::SetHadFormInteraction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  document_.had_form_interaction.SetAndMaybeNotify(this, true);
}

void FrameNodeImpl::SetHadUserEdits() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  document_.had_user_edits.SetAndMaybeNotify(this, true);
}

void FrameNodeImpl::OnStartedUsingWebRTC() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  document_.uses_web_rtc.SetAndMaybeNotify(this, true);
}

void FrameNodeImpl::OnStoppedUsingWebRTC() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  document_.uses_web_rtc.SetAndMaybeNotify(this, false);
}

void FrameNodeImpl::OnNonPersistentNotificationCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : GetObservers()) {
    observer.OnNonPersistentNotificationCreated(this);
  }
}

void FrameNodeImpl::OnFirstContentfulPaint(
    base::TimeDelta time_since_navigation_start) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : GetObservers()) {
    observer.OnFirstContentfulPaint(this, time_since_navigation_start);
  }
}

void FrameNodeImpl::OnWebMemoryMeasurementRequested(
    mojom::WebMemoryMeasurement::Mode mode,
    OnWebMemoryMeasurementRequestedCallback callback) {
  v8_memory::WebMeasureMemory(
      this, mode, v8_memory::WebMeasureMemorySecurityChecker::Create(),
      std::move(callback), mojo::GetBadMessageCallback());
}

const blink::LocalFrameToken& FrameNodeImpl::GetFrameToken() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return frame_token_;
}

content::BrowsingInstanceId FrameNodeImpl::GetBrowsingInstanceId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browsing_instance_id_;
}

content::SiteInstanceGroupId FrameNodeImpl::GetSiteInstanceGroupId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return site_instance_group_id_;
}

resource_attribution::FrameContext FrameNodeImpl::GetResourceContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resource_attribution::FrameContext::FromFrameNode(this);
}

bool FrameNodeImpl::IsMainFrame() const {
  return !parent_frame_node_;
}

FrameNodeImpl::LifecycleState FrameNodeImpl::GetLifecycleState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state_.value();
}

bool FrameNodeImpl::HasNonemptyBeforeUnload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.has_nonempty_beforeunload;
}

const GURL& FrameNodeImpl::GetURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.url;
}

const std::optional<url::Origin>& FrameNodeImpl::GetOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.origin;
}

bool FrameNodeImpl::IsCurrent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_current_;
}

const PriorityAndReason& FrameNodeImpl::GetPriorityAndReason() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return priority_and_reason_.value();
}

bool FrameNodeImpl::GetNetworkAlmostIdle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.network_almost_idle.value();
}

bool FrameNodeImpl::IsAdFrame() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_ad_frame_.value();
}

bool FrameNodeImpl::IsHoldingWebLock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_weblock_.value();
}

bool FrameNodeImpl::IsHoldingIndexedDBLock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_indexeddb_lock_.value();
}

bool FrameNodeImpl::UsesWebRTC() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.uses_web_rtc.value();
}

bool FrameNodeImpl::HadUserActivation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return had_user_activation_.value();
}

bool FrameNodeImpl::HadFormInteraction() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.had_form_interaction.value();
}

bool FrameNodeImpl::HadUserEdits() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return document_.had_user_edits.value();
}

bool FrameNodeImpl::IsAudible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_audible_.value();
}

bool FrameNodeImpl::IsCapturingMediaStream() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_capturing_media_stream_.value();
}

std::optional<ViewportIntersection> FrameNodeImpl::GetViewportIntersection()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The outermost main frame or embedder is always fully intersecting with the
  // viewport.
  if (!parent_or_outer_document_or_embedder()) {
    return std::make_optional<ViewportIntersection>(
        ViewportIntersection::CreateIntersecting(
            /*is_intersecting_large_area=*/true));
  }
  return viewport_intersection_.value();
}

FrameNode::Visibility FrameNodeImpl::GetVisibility() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return visibility_.value();
}

bool FrameNodeImpl::IsImportant() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_important_.value();
}

const RenderFrameHostProxy& FrameNodeImpl::GetRenderFrameHostProxy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_frame_host_proxy_;
}

uint64_t FrameNodeImpl::GetResidentSetKbEstimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resident_set_kb_estimate_;
}

uint64_t FrameNodeImpl::GetPrivateFootprintKbEstimate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return private_footprint_kb_estimate_;
}

FrameNodeImpl* FrameNodeImpl::parent_frame_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent_frame_node_;
}

FrameNodeImpl* FrameNodeImpl::parent_or_outer_document_or_embedder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_frame_node_) {
    return parent_frame_node_;
  }

  if (outer_document_for_fenced_frame_) {
    return outer_document_for_fenced_frame_;
  }

  if (page_node()->embedder_frame_node()) {
    return page_node()->embedder_frame_node();
  }

  return nullptr;
}

PageNodeImpl* FrameNodeImpl::page_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_node_;
}

ProcessNodeImpl* FrameNodeImpl::process_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node_;
}

int FrameNodeImpl::render_frame_id() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return render_frame_id_;
}

FrameNode::NodeSetView<FrameNodeImpl*> FrameNodeImpl::child_frame_nodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<FrameNodeImpl*>(child_frame_nodes_);
}

FrameNode::NodeSetView<PageNodeImpl*> FrameNodeImpl::opened_page_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<PageNodeImpl*>(opened_page_nodes_);
}

FrameNode::NodeSetView<PageNodeImpl*> FrameNodeImpl::embedded_page_nodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<PageNodeImpl*>(embedded_page_nodes_);
}

FrameNode::NodeSetView<WorkerNodeImpl*> FrameNodeImpl::child_worker_nodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<WorkerNodeImpl*>(child_worker_nodes_);
}

// static
void FrameNodeImpl::UpdateCurrentFrame(FrameNodeImpl* previous_frame_node,
                                       FrameNodeImpl* current_frame_node,
                                       GraphImpl* graph) {
  if (previous_frame_node) {
    bool did_change = previous_frame_node->SetIsCurrent(false);
    // Don't notify if the frame was already not current.
    if (!did_change) {
      previous_frame_node = nullptr;
    }
  }

  if (current_frame_node) {
    bool did_change = current_frame_node->SetIsCurrent(true);
    // Don't notify if the frame was already current.
    if (!did_change) {
      current_frame_node = nullptr;
    }
  }

  // No need to notify observers.
  if (!previous_frame_node && !current_frame_node) {
    return;
  }

  // Notify observers.
  for (auto& observer : graph->GetObservers<FrameNodeObserver>()) {
    observer.OnCurrentFrameChanged(previous_frame_node, current_frame_node);
  }

  // TODO(crbug.com/40182881): We maintain an invariant that of all sibling
  // frame nodes in the same FrameTreeNode, at most one may be current. We used
  // to save the RenderFrameHost's `frame_tree_node_id` at FrameNode creation
  // time to check this invariant, but prerendering RenderFrameHost's can be
  // moved to a new FrameTreeNode when they're activated so the
  // `frame_tree_node_id` can go out of date. Because of this,
  // RenderFrameHost::GetFrameTreeNodeId() is being deprecated. (See the
  // discussion at crbug.com/1179502 and in the comment thread at
  // https://chromium-review.googlesource.com/c/chromium/src/+/2966195/comments/58550eac_5795f790
  // for more details.) We need to find another way to check this invariant
  // here. (altimin suggests simply relying on RFH::GetLifecycleState to
  // correctly track "active" frame nodes instead of using "current", and not
  // checking this invariant.)
}

void FrameNodeImpl::SetHadUserActivation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  had_user_activation_.SetAndMaybeNotify(this, true);
}

void FrameNodeImpl::SetIsHoldingWebLock(bool is_holding_weblock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(is_holding_weblock, is_holding_weblock_.value());
  is_holding_weblock_.SetAndMaybeNotify(this, is_holding_weblock);
}

void FrameNodeImpl::SetIsHoldingIndexedDBLock(bool is_holding_indexeddb_lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(is_holding_indexeddb_lock, is_holding_indexeddb_lock_.value());
  is_holding_indexeddb_lock_.SetAndMaybeNotify(this, is_holding_indexeddb_lock);
}

void FrameNodeImpl::SetIsAudible(bool is_audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(is_audible, is_audible_.value());
  is_audible_.SetAndMaybeNotify(this, is_audible);
}

void FrameNodeImpl::SetIsCapturingMediaStream(bool is_capturing_media_stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(is_capturing_media_stream, is_capturing_media_stream_.value());
  is_capturing_media_stream_.SetAndMaybeNotify(this, is_capturing_media_stream);
}

void FrameNodeImpl::SetViewportIntersection(
    const blink::mojom::ViewportIntersectionState&
        viewport_intersection_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  has_viewport_intersection_updates_ = true;

  const gfx::Rect& viewport_intersection =
      viewport_intersection_state.viewport_intersection;
  if (viewport_intersection.IsEmpty()) {
    SetViewportIntersectionImpl(ViewportIntersection::CreateNotIntersecting());
    return;
  }

  int viewport_intersect_area =
      viewport_intersection.size().GetCheckedArea().ValueOrDefault(INT_MAX);
  int outermost_main_frame_area =
      viewport_intersection_state.outermost_main_frame_size.GetCheckedArea()
          .ValueOrDefault(INT_MAX);
  float ratio = 1.0f * viewport_intersect_area / outermost_main_frame_area;
  const float ratio_threshold =
      blink::features::kLargeFrameSizePercentThreshold.Get() / 100.f;

  bool is_intersecting_large_area = ratio > ratio_threshold;
  SetViewportIntersectionImpl(
      ViewportIntersection::CreateIntersecting(is_intersecting_large_area));
}

void FrameNodeImpl::SetViewportIntersection(
    blink::mojom::FrameVisibility frame_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If this frame is getting blink::mojom::ViewportIntersectionState updates,
  // then ignore blink::mojom::FrameVisibility updates. The latter is basically
  // a subset of the former.
  if (has_viewport_intersection_updates_) {
    return;
  }

  bool is_intersecting_viewport = [&]() {
    switch (frame_visibility) {
      case blink::mojom::FrameVisibility::kNotRendered:
      case blink::mojom::FrameVisibility::kRenderedOutOfViewport:
        return false;
      case blink::mojom::FrameVisibility::kRenderedInViewport:
        // Since we don't know if this frame is intersecting with a large area
        // of the viewport, it'll be inherited from the parent.
        return true;
    }
    NOTREACHED();
  }();

  SetViewportIntersectionImpl(is_intersecting_viewport);
}

void FrameNodeImpl::SetInitialVisibility(Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visibility_.Set(this, visibility);
}

void FrameNodeImpl::SetVisibility(Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visibility_.SetAndMaybeNotify(this, visibility);
}

void FrameNodeImpl::SetIsImportant(bool is_important) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_important_.SetAndMaybeNotify(this, is_important);
}

void FrameNodeImpl::SetResidentSetKbEstimate(uint64_t rss_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resident_set_kb_estimate_ = rss_estimate;
}

void FrameNodeImpl::SetPrivateFootprintKbEstimate(
    uint64_t private_footprint_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  private_footprint_kb_estimate_ = private_footprint_estimate;
}

void FrameNodeImpl::OnNavigationCommitted(
    GURL url,
    url::Origin origin,
    bool same_document,
    bool is_served_from_back_forward_cache) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (same_document) {
    url = std::exchange(document_.url, std::move(url));

    if (url != document_.url) {
      for (auto& observer : GetObservers()) {
        observer.OnURLChanged(this, /*previous_value=*/url);
      }
    }

    return;
  }

  // If this frame is being served from the back-forward cache, then this frame
  // does not host a new document. We don't need to reset the document's
  // properties, and more importantly, we can't reset the `receiver_` as there
  // won't be another Bind() request to rebind it.
  if (is_served_from_back_forward_cache) {
    return;
  }

  // Close |receiver_| to ensure that messages queued by the previous document
  // before the navigation commit are dropped.
  //
  // Note: It is guaranteed that |receiver_| isn't yet bound to the new
  // document.
  //       This is important because it would be incorrect to close the new
  //       document's binding.
  //
  //       Renderer: blink::DocumentLoader::DidCommitNavigation
  //                   ... content::RenderFrameImpl::DidCommitProvisionalLoad
  //                     ... mojom::FrameHost::DidCommitProvisionalLoad
  //       Browser:  RenderFrameHostImpl::DidCommitNavigation
  //                   Bind the new document's interface provider [A]
  //                   PMTabHelper::DidFinishNavigation
  //                     (async) FrameNodeImpl::OnNavigationCommitted [B]
  //       Renderer: Request DocumentCoordinationUnit interface
  //       Browser:  PMTabHelper::OnInterfaceRequestFromFrame [C]
  //                   (async) FrameNodeImpl::Bind [D]
  //
  //       A happens before C, because no interface request can be processed
  //       before the interface provider is bound. A posts B to PM sequence and
  //       C posts D to PM sequence, therefore B happens before D.
  receiver_.reset();

  // Reset properties.
  document_.Reset(this, std::move(url), std::move(origin));
}

void FrameNodeImpl::AddChildWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = child_worker_nodes_.insert(worker_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveChildWorker(WorkerNodeImpl* worker_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t removed = child_worker_nodes_.erase(worker_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::SetPriorityAndReason(
    const PriorityAndReason& priority_and_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This is also called during initialization to set the initial value. In this
  // case, do not notify the observers as they aren't even aware of this frame
  // node anyways.
  if (CanSetProperty()) {
    priority_and_reason_.Set(this, priority_and_reason);
    return;
  }
  priority_and_reason_.SetAndMaybeNotify(this, priority_and_reason);
}

base::WeakPtr<FrameNodeImpl> FrameNodeImpl::GetWeakPtrOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_this_;
}

base::WeakPtr<FrameNodeImpl> FrameNodeImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void FrameNodeImpl::AddOpenedPage(base::PassKey<PageNodeImpl>,
                                  PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_node);
  DCHECK_NE(page_node_, page_node);
  DCHECK(graph()->NodeInGraph(page_node));
  DCHECK_EQ(this, page_node->opener_frame_node());
  bool inserted = opened_page_nodes_.insert(page_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveOpenedPage(base::PassKey<PageNodeImpl>,
                                     PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_node);
  DCHECK_NE(page_node_, page_node);
  DCHECK(graph()->NodeInGraph(page_node));
  DCHECK_EQ(this, page_node->opener_frame_node());
  size_t removed = opened_page_nodes_.erase(page_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::AddEmbeddedPage(base::PassKey<PageNodeImpl>,
                                    PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_node);
  DCHECK_NE(page_node_, page_node);
  DCHECK(graph()->NodeInGraph(page_node));
  DCHECK_EQ(this, page_node->embedder_frame_node());
  bool inserted = embedded_page_nodes_.insert(page_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveEmbeddedPage(base::PassKey<PageNodeImpl>,
                                       PageNodeImpl* page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(page_node);
  DCHECK_NE(page_node_, page_node);
  DCHECK(graph()->NodeInGraph(page_node));
  DCHECK_EQ(this, page_node->embedder_frame_node());
  size_t removed = embedded_page_nodes_.erase(page_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::SetViewportIntersectionForTesting(
    bool is_intersecting_viewport) {
  SetViewportIntersectionImpl(is_intersecting_viewport);
}

void FrameNodeImpl::SetViewportIntersectionForTesting(
    ViewportIntersection viewport_intersection) {
  SetViewportIntersectionImpl(viewport_intersection);
}

const FrameNode* FrameNodeImpl::GetParentFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent_frame_node();
}

const FrameNode* FrameNodeImpl::GetParentOrOuterDocumentOrEmbedder() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return parent_or_outer_document_or_embedder();
}

const PageNode* FrameNodeImpl::GetPageNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_node();
}

const ProcessNode* FrameNodeImpl::GetProcessNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_node();
}

FrameNode::NodeSetView<const FrameNode*> FrameNodeImpl::GetChildFrameNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const FrameNode*>(child_frame_nodes_);
}

FrameNode::NodeSetView<const PageNode*> FrameNodeImpl::GetOpenedPageNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const PageNode*>(opened_page_nodes_);
}

FrameNode::NodeSetView<const PageNode*> FrameNodeImpl::GetEmbeddedPageNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const PageNode*>(embedded_page_nodes_);
}

FrameNode::NodeSetView<const WorkerNode*> FrameNodeImpl::GetChildWorkerNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<const WorkerNode*>(child_worker_nodes_);
}

void FrameNodeImpl::AddChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->parent_frame_node());
  DCHECK_NE(this, child_frame_node);
  DCHECK(graph()->NodeInGraph(child_frame_node));
  DCHECK(!HasFrameNodeInAncestors(child_frame_node) &&
         !child_frame_node->HasFrameNodeInDescendants(this));

  bool inserted = child_frame_nodes_.insert(child_frame_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->parent_frame_node());
  DCHECK_NE(this, child_frame_node);
  DCHECK(graph()->NodeInGraph(child_frame_node));

  size_t removed = child_frame_nodes_.erase(child_frame_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::OnJoiningGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure all weak pointers, even `weak_this_` that was created on the UI
  // thread in the constructor, can only be dereferenced on the graph sequence.
  weak_factory_.BindToCurrentSequence(
      base::subtle::BindWeakPtrFactoryPassKey());

  NodeAttachedDataStorage::Create(this);
  execution_context::FrameExecutionContext::Create(this, this);

  // Enable querying this node using process and frame routing ids.
  graph()->RegisterFrameNodeForId(process_node_->GetRenderProcessHostId(),
                                  render_frame_id_, this);

  // Notify the initializing observers.
  graph()->NotifyFrameNodeInitializing(this);

  // Wire this up to the other nodes in the graph.
  if (parent_frame_node_)
    parent_frame_node_->AddChildFrame(this);
  page_node_->AddFrame(base::PassKey<FrameNodeImpl>(), this);
  process_node_->AddFrame(this);
}

void FrameNodeImpl::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(child_frame_nodes_.empty());

  SeverPageRelationshipsAndMaybeReparent();

  // Leave the page.
  DCHECK(graph()->NodeInGraph(page_node_));
  page_node_->RemoveFrame(base::PassKey<FrameNodeImpl>(), this);

  // Leave the frame hierarchy.
  if (parent_frame_node_) {
    DCHECK(graph()->NodeInGraph(parent_frame_node_));
    parent_frame_node_->RemoveChildFrame(this);
  }

  // And leave the process.
  DCHECK(graph()->NodeInGraph(process_node_));
  process_node_->RemoveFrame(this);

  // Notify the initializing observers for cleanup.
  graph()->NotifyFrameNodeTearingDown(this);

  // Disable querying this node using process and frame routing ids.
  graph()->UnregisterFrameNodeForId(process_node_->GetRenderProcessHostId(),
                                    render_frame_id_, this);
}

void FrameNodeImpl::RemoveNodeAttachedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyNodeInlineDataStorage();
}

void FrameNodeImpl::SeverPageRelationshipsAndMaybeReparent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Be careful when iterating: when we call
  // PageNodeImpl::(Set|Clear)(Opener|Embedder)FrameNode() this will call
  // back into this frame node and call Remove(Opened|Embedded)Page(), which
  // modifies |opened_page_nodes_| and |embedded_page_nodes_|.
  //
  // We also reparent related pages to this frame's parent to maintain the
  // relationship between the distinct frame trees for bookkeeping. For the
  // relationship to be finally severed one of the frame trees must completely
  // disappear.
  NodeSet opened_page_nodes_copy = opened_page_nodes_;
  for (const Node* opened_page_node : opened_page_nodes_copy) {
    PageNodeImpl* opened_page = PageNodeImpl::FromNode(opened_page_node);
    if (parent_frame_node_) {
      opened_page->SetOpenerFrameNode(parent_frame_node_);
    } else {
      opened_page->ClearOpenerFrameNode();
    }
  }

  NodeSet embedded_page_nodes_copy = embedded_page_nodes_;
  for (const Node* embedded_page_node : embedded_page_nodes_copy) {
    PageNodeImpl* embedded_page = PageNodeImpl::FromNode(embedded_page_node);
    auto embedding_type = embedded_page->GetEmbeddingType();
    if (parent_frame_node_) {
      embedded_page->SetEmbedderFrameNodeAndEmbeddingType(parent_frame_node_,
                                                          embedding_type);
    } else {
      embedded_page->ClearEmbedderFrameNodeAndEmbeddingType();
    }
  }

  // Expect each page node to have called RemoveEmbeddedPage(), and for this to
  // now be empty.
  DCHECK(opened_page_nodes_.empty());
  DCHECK(embedded_page_nodes_.empty());
}

FrameNodeImpl* FrameNodeImpl::GetFrameTreeRoot() const {
  FrameNodeImpl* root = const_cast<FrameNodeImpl*>(this);
  while (root->parent_frame_node())
    root = parent_frame_node();
  DCHECK_NE(nullptr, root);
  return root;
}

bool FrameNodeImpl::HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_frame_node_ == frame_node ||
      (parent_frame_node_ &&
       parent_frame_node_->HasFrameNodeInAncestors(frame_node))) {
    return true;
  }
  return false;
}

bool FrameNodeImpl::HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (FrameNodeImpl* child : child_frame_nodes()) {
    if (child == frame_node || child->HasFrameNodeInDescendants(frame_node)) {
      return true;
    }
  }
  return false;
}

bool FrameNodeImpl::HasFrameNodeInTree(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFrameTreeRoot() == frame_node->GetFrameTreeRoot();
}

bool FrameNodeImpl::SetIsCurrent(bool is_current) {
  CHECK(CanSetAndNotifyProperty());
  bool was_current = std::exchange(is_current_, is_current);
  return was_current != is_current_;
}

void FrameNodeImpl::SetViewportIntersectionImpl(bool is_intersecting_viewport) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The outermost main frame or embedder is always fully intersecting with the
  // viewport, so it is not tracked.
  CHECK(parent_or_outer_document_or_embedder());

  ViewportIntersection viewport_intersection = [&, this]() {
    if (is_intersecting_viewport) {
      // An intersecting viewport intersection needs to inherit its
      // `is_intersecting_large_area` bit from its parent.
      return ViewportIntersection::CreateIntersecting(
          IsParentIntersectingLargeArea(this));
    } else {
      return ViewportIntersection::CreateNotIntersecting();
    }
  }();

  SetViewportIntersectionImpl(viewport_intersection);
}

void FrameNodeImpl::SetViewportIntersectionImpl(
    ViewportIntersection viewport_intersection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Nothing to do if the value didn't change.
  if (GetViewportIntersection() &&
      GetViewportIntersection().value() == viewport_intersection) {
    return;
  }

  viewport_intersection_.SetAndMaybeNotify(this, viewport_intersection);

  // Child frames can be inheriting the `is_intersecting_large_area` bit from
  // their parent. Update them if this frame's value change.
  if (viewport_intersection.is_intersecting()) {
    for (FrameNodeImpl* child_frame_node : child_frame_nodes()) {
      if (child_frame_node->process_node() == process_node()) {
        child_frame_node->SetInheritedIsIntersectingLargeArea(
            viewport_intersection.is_intersecting_large_area());
      }
    }
  }
}

void FrameNodeImpl::SetInheritedIsIntersectingLargeArea(
    bool is_intersecting_large_area) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Since this frame's viewport intersection is derived from an accurate
  // blink::mojom::ViewportIntersectionState update, it doesn't have to inherit
  // the `is_intersecting_large_area` bit from its parent.
  if (has_viewport_intersection_updates_) {
    return;
  }

  std::optional<ViewportIntersection> viewport_intersection =
      GetViewportIntersection();
  if (!viewport_intersection) {
    return;
  }

  if (!viewport_intersection->is_intersecting()) {
    return;
  }

  SetViewportIntersectionImpl(
      ViewportIntersection::CreateIntersecting(is_intersecting_large_area));
}

FrameNodeImpl::DocumentProperties::DocumentProperties() = default;
FrameNodeImpl::DocumentProperties::~DocumentProperties() = default;

void FrameNodeImpl::DocumentProperties::Reset(FrameNodeImpl* frame_node,
                                              GURL url_in,
                                              url::Origin origin_in) {
  // Update the URL and origin properties.
  url_in = std::exchange(url, std::move(url_in));

  std::optional<url::Origin> previous_origin = std::move(origin);
  origin = std::move(origin_in);

  // Notify observers of the URL and origin change after updating both
  // properties, to avoid having observers that mistakenly access the URL and
  // origin of different documents.
  if (url != url_in) {
    for (auto& observer : frame_node->GetObservers()) {
      observer.OnURLChanged(frame_node, /*previous_value=*/url_in);
    }
  }

  if (origin != previous_origin) {
    for (auto& observer : frame_node->GetObservers()) {
      observer.OnOriginChanged(frame_node, /*previous_value=*/previous_origin);
    }
  }

  // Update other properties.
  has_nonempty_beforeunload = false;
  // Network is busy on navigation.
  network_almost_idle.SetAndMaybeNotify(frame_node, false);
  had_form_interaction.SetAndMaybeNotify(frame_node, false);
  had_user_edits.SetAndMaybeNotify(frame_node, false);
}

}  // namespace performance_manager
