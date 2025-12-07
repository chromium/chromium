// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/page_node_impl.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/graph_operations.h"
#include "components/performance_manager/public/render_frame_host_proxy.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace performance_manager {

namespace {

using perfetto::protos::pbzero::ChromeTrackEvent;

// Creates a collapsible global track to hold all PageNode tracing tracks.
perfetto::NamedTrack CreatePageGroupTrack() {
  return perfetto::NamedTrack("PageNodes", 0, perfetto::Track::Global(0));
}

// Creates a tracing track for the PageNode identified by `token`.
perfetto::NamedTrack CreatePageNodeTrack(const base::UnguessableToken& token) {
  static const base::NoDestructor<
      base::trace_event::TrackRegistration<perfetto::NamedTrack>>
      page_group_track(CreatePageGroupTrack());
  auto track =
      perfetto::NamedTrack("PageNode", base::UnguessableTokenHash()(token),
                           page_group_track->track())
          .disable_sibling_merge();
  return track;
}

perfetto::StaticString PageNodeLoadingStateToString(
    const PageNode::LoadingState& loading_state) {
  switch (loading_state) {
    case PageNode::LoadingState::kLoadingNotStarted:
      return nullptr;
    case PageNode::LoadingState::kLoading:
      return "Loading";
    case PageNode::LoadingState::kLoadingTimedOut:
      return "LoadingTimedOut";
    case PageNode::LoadingState::kLoadedBusy:
      return "LoadedBusy";
    case PageNode::LoadingState::kLoadedIdle:
      return nullptr;
  }
  NOTREACHED();
}

perfetto::StaticString PageNodeVisibilityToString(const bool& is_visible) {
  if (is_visible) {
    return "Visible";
  } else {
    return "Not Visible";
  }
}

}  // namespace

PageNodeImpl::PageNodeImpl(base::WeakPtr<content::WebContents> web_contents,
                           const std::string& browser_context_id,
                           const GURL& visible_url,
                           PagePropertyFlags initial_properties,
                           base::TimeTicks visibility_change_time)
    : web_contents_(std::move(web_contents)),
      tracing_track_(CreatePageNodeTrack(page_token_.value())),
      loading_track_("Loading", 0, *tracing_track_),
      visibility_change_time_(visibility_change_time),
      main_frame_url_(visible_url),
      browser_context_id_(browser_context_id),
      is_focused_(false,
                  perfetto::NamedTrack("IsFocused", 0, *tracing_track_),
                  YesNoStateToString),
      is_visible_(initial_properties.Has(PagePropertyFlag::kIsVisible),
                  *tracing_track_,
                  PageNodeVisibilityToString),
      is_audible_(initial_properties.Has(PagePropertyFlag::kIsAudible),
                  perfetto::NamedTrack("IsAudible", 0, *tracing_track_),
                  YesNoStateToString),
      has_picture_in_picture_(
          initial_properties.Has(PagePropertyFlag::kHasPictureInPicture)),
      is_off_the_record_(
          initial_properties.Has(PagePropertyFlag::kIsOffTheRecord)),
      loading_state_(LoadingState::kLoadingNotStarted,
                     loading_track_,
                     PageNodeLoadingStateToString) {
  // The `PageNodeImpl` creation hook is before the `WebContents`' visible or
  // committed url can be set, so the initial main frame URL is always empty.
  // TODO(crbug.com/40121561): Remove `visible_url` from the constructor in M132
  // if no issues are found with this CHECK.
  CHECK(main_frame_url_.value().is_empty());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_audible_.value()) {
    audible_change_time_ = base::TimeTicks::Now();
  }
}

PageNodeImpl::~PageNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(nullptr, opener_frame_node_);
  DCHECK_EQ(nullptr, embedder_frame_node_);
}

const std::string& PageNodeImpl::GetBrowserContextID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return browser_context_id_;
}

resource_attribution::PageContext PageNodeImpl::GetResourceContext() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resource_attribution::PageContext::FromPageNode(this);
}

PageType PageNodeImpl::GetType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return type_.value();
}

bool PageNodeImpl::IsFocused() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_focused_.value();
}

bool PageNodeImpl::IsVisible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_visible_.value();
}

base::TimeTicks PageNodeImpl::GetLastVisibilityChangeTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return visibility_change_time_;
}

bool PageNodeImpl::IsAudible() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_audible_.value();
}

std::optional<base::TimeDelta> PageNodeImpl::GetTimeSinceLastAudibleChange()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (audible_change_time_.has_value()) {
    return base::TimeTicks::Now() - audible_change_time_.value();
  }
  return std::nullopt;
}

bool PageNodeImpl::HasPictureInPicture() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_picture_in_picture_.value();
}

bool PageNodeImpl::HasFreezingOriginTrialOptOut() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_freezing_origin_trial_opt_out_.value();
}

bool PageNodeImpl::IsOffTheRecord() const {
  return is_off_the_record_;
}

PageNode::LoadingState PageNodeImpl::GetLoadingState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loading_state_.value();
}

ukm::SourceId PageNodeImpl::GetUkmSourceID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ukm_source_id_.value();
}

PageNodeImpl::LifecycleState PageNodeImpl::GetLifecycleState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state_.value();
}

bool PageNodeImpl::IsHoldingWebLock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_weblock_.value();
}

bool PageNodeImpl::IsHoldingBlockingIndexedDBLock() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_holding_blocking_indexeddb_lock_.value();
}

bool PageNodeImpl::UsesWebRTC() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return uses_web_rtc_.value();
}

int64_t PageNodeImpl::GetNavigationID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return navigation_id_;
}

const std::string& PageNodeImpl::GetContentsMimeType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return contents_mime_type_;
}

std::optional<blink::mojom::PermissionStatus>
PageNodeImpl::GetNotificationPermissionStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return notification_permission_status_.value();
}

base::TimeDelta PageNodeImpl::GetTimeSinceLastNavigation() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (navigation_committed_time_.is_null()) {
    return base::TimeDelta();
  }
  return base::TimeTicks::Now() - navigation_committed_time_;
}

const GURL& PageNodeImpl::GetMainFrameUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return main_frame_url_.value();
}

base::ByteCount PageNodeImpl::EstimateMainFramePrivateFootprintSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ByteCount total;
  FrameNodeImpl* main_frame = main_frame_node();
  if (main_frame) {
    performance_manager::GraphImplOperations::VisitFrameAndChildrenPreOrder(
        main_frame, [&total](FrameNodeImpl* frame_node) {
          total += frame_node->GetPrivateFootprintEstimate();
          return true;
        });
  }
  return total;
}

bool PageNodeImpl::HadFormInteraction() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return had_form_interaction_.value();
}

bool PageNodeImpl::HadUserEdits() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return had_user_edits_.value();
}

base::WeakPtr<content::WebContents> PageNodeImpl::GetWebContents() const {
  return web_contents_;
}

base::ByteCount PageNodeImpl::EstimateResidentSetSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ByteCount total;
  performance_manager::GraphOperations::VisitFrameTreePreOrder(
      this, [&total](const FrameNode* frame_node) {
        total += frame_node->GetResidentSetEstimate();
        return true;
      });
  return total;
}

base::ByteCount PageNodeImpl::EstimatePrivateFootprintSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ByteCount total;
  performance_manager::GraphOperations::VisitFrameTreePreOrder(
      this, [&total](const FrameNode* frame_node) {
        total += frame_node->GetPrivateFootprintEstimate();
        return true;
      });
  return total;
}

base::WeakPtr<PageNode> PageNodeImpl::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<const PageNode> PageNodeImpl::GetWeakPtr() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void PageNodeImpl::AddFrame(base::PassKey<FrameNodeImpl>,
                            FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->page_node());
  DCHECK(graph()->NodeInGraph(frame_node));

  ++frame_node_count_;

  if (frame_node->parent_frame_node() == nullptr) {
    main_frame_nodes_.insert(frame_node);
  }
}

void PageNodeImpl::TraceFrame(base::PassKey<FrameNodeImpl>,
                              FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK(graph()->NodeInGraph(frame_node));
  auto frame_track = perfetto::NamedTrack::FromPointer("FrameNodes", frame_node,
                                                       tracing_track());
  // Unmatched end event is a no-op.
  TRACE_EVENT_END("performance_manager.graph", frame_track);
  TRACE_EVENT_BEGIN(
      "performance_manager.graph",
      perfetto::StaticString(frame_node->IsMainFrame() ? "MainFrame" : "Frame"),
      frame_track, perfetto::TerminatingFlow::FromPointer(frame_node));
}

void PageNodeImpl::RemoveFrame(base::PassKey<FrameNodeImpl>,
                               FrameNodeImpl* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_node);
  DCHECK_EQ(this, frame_node->page_node());
  DCHECK(graph()->NodeInGraph(frame_node));

  --frame_node_count_;
  if (frame_node->parent_frame_node() == nullptr) {
    size_t removed = main_frame_nodes_.erase(frame_node);
    DCHECK_EQ(1u, removed);
  }
  auto frame_track = perfetto::NamedTrack::FromPointer("FrameNodes", frame_node,
                                                       tracing_track());
  TRACE_EVENT_END("performance_manager.graph", frame_track);
}

void PageNodeImpl::SetLoadingState(LoadingState loading_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loading_state_.SetAndMaybeNotify(this, loading_state);
}

void PageNodeImpl::SetType(PageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  type_.SetAndMaybeNotify(this, type);
}

void PageNodeImpl::SetIsFocused(bool is_focused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_focused_.SetAndMaybeNotify(this, is_focused);
}

void PageNodeImpl::SetIsVisible(bool is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_visible_.SetAndMaybeNotify(this, is_visible)) {
    // The change time needs to be updated after observers are notified, as they
    // use this to determine time passed since the *previous* visibility state
    // change. They can infer the current state change time themselves via
    // NowTicks.
    visibility_change_time_ = base::TimeTicks::Now();
  }
}

void PageNodeImpl::SetIsAudible(bool is_audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_audible_.SetAndMaybeNotify(this, is_audible)) {
    // The change time needs to be updated after observers are notified, as they
    // use this to determine time passed since the *previous* state change. They
    // can infer the current state change time themselves via NowTicks.
    audible_change_time_ = base::TimeTicks::Now();
  }
}

void PageNodeImpl::SetHasPictureInPicture(bool has_picture_in_picture) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_picture_in_picture_.SetAndMaybeNotify(this, has_picture_in_picture);
}

void PageNodeImpl::SetUkmSourceId(ukm::SourceId ukm_source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ukm_source_id_.SetAndMaybeNotify(this, ukm_source_id);
}

void PageNodeImpl::OnFaviconUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : GetObservers()) {
    observer.OnFaviconUpdated(this);
  }
}

void PageNodeImpl::OnTitleUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : GetObservers()) {
    observer.OnTitleUpdated(this);
  }
}

void PageNodeImpl::OnAboutToBeDiscarded(base::WeakPtr<PageNode> new_page_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!embedder_frame_node_)
      << "Discard should only be called on the primary page node.";

  if (!new_page_node) {
    return;
  }

  // Notify all embedded frames that the primary page is about to be discarded.
  if (FrameNodeImpl* main_frame = main_frame_node()) {
    main_frame->OnPrimaryPageAboutToBeDiscarded();
  }

  for (auto& observer : GetObservers()) {
    observer.OnAboutToBeDiscarded(this, new_page_node.get());
  }
}

void PageNodeImpl::SetMainFrameRestoredState(
    const GURL& url,
    blink::mojom::PermissionStatus notification_permission_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(main_frame_url_.value().is_empty());
  EmitMainFrameUrlChangedEvent(url);
  main_frame_url_.SetAndMaybeNotify(this, url);
  notification_permission_status_.SetAndMaybeNotify(
      this, notification_permission_status);
}

void PageNodeImpl::OnMainFrameNavigationCommitted(
    bool same_document,
    base::TimeTicks navigation_committed_time,
    int64_t navigation_id,
    const GURL& url,
    const std::string& contents_mime_type,
    std::optional<blink::mojom::PermissionStatus>
        notification_permission_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should never be invoked with a null navigation, nor should it be
  // called twice for the same navigation.
  DCHECK_NE(0, navigation_id);
  DCHECK_NE(navigation_id_, navigation_id);
  navigation_committed_time_ = navigation_committed_time;
  navigation_id_ = navigation_id;
  contents_mime_type_ = contents_mime_type;
  if (url != main_frame_url_.value()) {
    EmitMainFrameUrlChangedEvent(url, navigation_id);
  }
  main_frame_url_.SetAndMaybeNotify(this, url);
  notification_permission_status_.SetAndMaybeNotify(
      this, notification_permission_status);

  // No mainframe document change notification on same-document navigations.
  if (same_document) {
    return;
  }

  for (auto& observer : GetObservers()) {
    observer.OnMainFrameDocumentChanged(this);
  }
}

void PageNodeImpl::OnNotificationPermissionStatusChange(
    blink::mojom::PermissionStatus permission_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  notification_permission_status_.SetAndMaybeNotify(this, permission_status);
}

FrameNodeImpl* PageNodeImpl::opener_frame_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return opener_frame_node_;
}

FrameNodeImpl* PageNodeImpl::embedder_frame_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return embedder_frame_node_;
}

FrameNodeImpl* PageNodeImpl::main_frame_node() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (main_frame_nodes_.empty()) {
    return nullptr;
  }

  // Return the current frame node if there is one. Iterating over this set is
  // fine because it is almost always of length 1 or 2.
  for (FrameNodeImpl* frame : main_frame_nodes()) {
    if (frame->IsCurrent()) {
      return frame;
    }
  }

  // Otherwise, return any old main frame node.
  return *main_frame_nodes().begin();
}

PageNode::NodeSetView<FrameNodeImpl*> PageNodeImpl::main_frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return NodeSetView<FrameNodeImpl*>(main_frame_nodes_);
}

void PageNodeImpl::SetOpenerFrameNode(FrameNodeImpl* opener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(opener);
  DCHECK(graph()->NodeInGraph(opener));
  DCHECK_NE(this, opener->page_node());

  auto* previous_opener = opener_frame_node_.get();
  if (previous_opener) {
    previous_opener->RemoveOpenedPage(PassKey(), this);
  }
  opener_frame_node_ = opener;
  opener->AddOpenedPage(PassKey(), this);

  for (auto& observer : GetObservers()) {
    observer.OnOpenerFrameNodeChanged(this, previous_opener);
  }
}

void PageNodeImpl::ClearOpenerFrameNode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(nullptr, opener_frame_node_);

  auto* previous_opener = opener_frame_node_.get();

  opener_frame_node_->RemoveOpenedPage(PassKey(), this);
  opener_frame_node_ = nullptr;

  for (auto& observer : GetObservers()) {
    observer.OnOpenerFrameNodeChanged(this, previous_opener);
  }
}

void PageNodeImpl::SetEmbedderFrameNode(FrameNodeImpl* embedder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(embedder);
  DCHECK(graph()->NodeInGraph(embedder));
  DCHECK_NE(this, embedder->page_node());

  auto* previous_embedder = embedder_frame_node_.get();

  if (previous_embedder) {
    previous_embedder->RemoveEmbeddedPage(PassKey(), this);
  }
  embedder_frame_node_ = embedder;
  embedder->AddEmbeddedPage(PassKey(), this);

  for (auto& observer : GetObservers()) {
    observer.OnEmbedderFrameNodeChanged(this, previous_embedder);
  }
}

void PageNodeImpl::ClearEmbedderFrameNode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(nullptr, embedder_frame_node_);

  auto* previous_embedder = embedder_frame_node_.get();

  embedder_frame_node_->RemoveEmbeddedPage(PassKey(), this);
  embedder_frame_node_ = nullptr;

  for (auto& observer : GetObservers()) {
    observer.OnEmbedderFrameNodeChanged(this, previous_embedder);
  }
}

void PageNodeImpl::set_has_nonempty_beforeunload(
    bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void PageNodeImpl::OnInitializingProperties() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeAttachedDataStorage::Create(this);
}

void PageNodeImpl::OnBeforeLeavingGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sever opener relationships.
  if (opener_frame_node_) {
    ClearOpenerFrameNode();
  }

  // Sever embedder relationships.
  if (embedder_frame_node_) {
    ClearEmbedderFrameNode();
  }

  DCHECK_EQ(0u, frame_node_count_);
}

void PageNodeImpl::CleanUpNodeState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DestroyNodeInlineDataStorage();
}

const FrameNode* PageNodeImpl::GetOpenerFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(graph()->NodeEdgesArePublic(this) || !opener_frame_node());
  return opener_frame_node();
}

const FrameNode* PageNodeImpl::GetEmbedderFrameNode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(graph()->NodeEdgesArePublic(this) || !embedder_frame_node());
  return embedder_frame_node();
}

const FrameNode* PageNodeImpl::GetMainFrameNode() const {
  CHECK(graph()->NodeEdgesArePublic(this) || !main_frame_node());
  return main_frame_node();
}

PageNode::NodeSetView<const FrameNode*> PageNodeImpl::GetMainFrameNodes()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(graph()->NodeEdgesArePublic(this) || main_frame_nodes_.empty());
  return NodeSetView<const FrameNode*>(main_frame_nodes_);
}

void PageNodeImpl::SetLifecycleState(LifecycleState lifecycle_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lifecycle_state_.SetAndMaybeNotify(this, lifecycle_state);
}

void PageNodeImpl::SetIsHoldingWebLock(bool is_holding_weblock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_holding_weblock_.SetAndMaybeNotify(this, is_holding_weblock);
}

void PageNodeImpl::SetIsHoldingBlockingIndexedDBLock(
    bool is_holding_blocking_indexeddb_lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_holding_blocking_indexeddb_lock_.SetAndMaybeNotify(
      this, is_holding_blocking_indexeddb_lock);
}

void PageNodeImpl::SetUsesWebRTC(bool uses_web_rtc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uses_web_rtc_.SetAndMaybeNotify(this, uses_web_rtc);
}

void PageNodeImpl::SetHadFormInteraction(bool had_form_interaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  had_form_interaction_.SetAndMaybeNotify(this, had_form_interaction);
}

void PageNodeImpl::SetHadUserEdits(bool had_user_edits) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  had_user_edits_.SetAndMaybeNotify(this, had_user_edits);
}

void PageNodeImpl::SetHasFreezingOriginTrialOptOut(
    bool has_freezing_origin_trial_opt_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_freezing_origin_trial_opt_out_.SetAndMaybeNotify(
      this, has_freezing_origin_trial_opt_out);
}

void PageNodeImpl::EmitMainFrameUrlChangedEvent(
    const GURL& url,
    std::optional<int64_t> navigation_id) const {
  TRACE_EVENT_INSTANT("performance_manager.graph", "MainFrameUrlChanged",
                      loading_track_, [&](perfetto::EventContext& ctx) {
                        perfetto::protos::pbzero::PageLoad* page_load =
                            ctx.event<ChromeTrackEvent>()->set_page_load();
                        page_load->set_url(url.possibly_invalid_spec());
                        if (navigation_id.has_value()) {
                          page_load->set_navigation_id(navigation_id.value());
                        }
                      });
}

}  // namespace performance_manager
