// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/shared/common/utils.h"
#include "components/subresource_filter/content/shared/renderer/web_document_subresource_filter_impl.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/time_measurements.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_ad_evidence.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace {

bool IsFencedFrameRoot(content::RenderFrame* frame) {
  // Unit tests may have a nullptr render_frame.
  if (!frame)
    return false;
  return frame->IsInFencedFrameTree() && frame->IsMainFrame();
}

}  // namespace

namespace subresource_filter {

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    UnverifiedRulesetDealer* ruleset_dealer)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SubresourceFilterAgent>(render_frame),
      ruleset_dealer_(ruleset_dealer) {
  CHECK(ruleset_dealer, base::NotFatalUntil::M129);
}

void SubresourceFilterAgent::Initialize() {
  const GURL& url = GetDocumentURL();
  // The initial empty document will always inherit activation.
  CHECK(ShouldInheritActivation(url), base::NotFatalUntil::M129);

  // We must check for provisional here because in that case 2 RenderFrames will
  // be created for the same FrameTreeNode in the browser. The browser service
  // only expects us to call SendFrameWasCreatedByAdScript() and
  // SendFrameIsAd() a single time each for a newly created RenderFrame,
  // so we must choose one. A provisional frame is created when a navigation is
  // performed cross-site and the navigation is done there to isolate it from
  // the previous frame tree. We choose to send this message from the initial
  // (non-provisional) "about:blank" frame that is created before the navigation
  // to match previous behaviour, and because this frame will always exist.
  // Whereas the provisional frame would only be created to perform the
  // navigation conditionally, so we ignore sending the IPC there.
  if (IsSubresourceFilterChild() && !IsFencedFrameRoot(render_frame()) &&
      !IsProvisional()) {
    // Note: We intentionally exclude fenced-frame roots here since they do not
    // create a RenderFrame in the creating renderer. By the time the fenced
    // frame initializes its RenderFrame in a new process,
    // IsFrameCreatedByAdScript will not see the creating call stack. Fenced
    // frames compute and send this information to the browser from
    // DidCreateFencedFrame which is called by the creating RenderFrame.
    // Additionally, there's no need to set evidence for the initial empty
    // subframe since the fenced frame is isolated from its embedder.
    if (IsFrameCreatedByAdScript())
      SendFrameWasCreatedByAdScript();

    // As this is the initial empty document, we won't have received any message
    // from the browser and so we must populate the ad evidence here.
    SetAdEvidenceForInitialEmptySubframe();
  }

  // `render_frame()` can be null in unit tests.
  if (render_frame()) {
    render_frame()
        ->GetAssociatedInterfaceRegistry()
        ->AddInterface<mojom::SubresourceFilterAgent>(base::BindRepeating(
            &SubresourceFilterAgent::OnSubresourceFilterAgentRequest,
            base::Unretained(this)));

    if (!IsSubresourceFilterChild()) {
      // If a root frame has an activated opener, we will activate the
      // subresource filter for the initial empty document, which was created
      // before the constructor for `this`. This ensures that a popup's final
      // document is appropriately activated, even when the the initial
      // navigation is aborted and there are no further documents created.
      // TODO(dcheng): Navigation is an asynchronous operation, and the opener
      // frame may have been destroyed between the time the window is opened
      // and the RenderFrame in the window is constructed leading us to here.
      // To avoid that race condition the activation state would need to be
      // determined without the use of the opener frame.
      if (GetInheritedActivationState(render_frame()).activation_level !=
          mojom::ActivationLevel::kDisabled) {
        ConstructFilter(GetInheritedActivationStateForNewDocument(), url);
      }
    } else {
      // Child frames always have a parent, so the empty initial document can
      // always inherit activation.
      ConstructFilter(GetInheritedActivationStateForNewDocument(), url);
    }
  }
}

SubresourceFilterAgent::~SubresourceFilterAgent() = default;

GURL SubresourceFilterAgent::GetDocumentURL() {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

bool SubresourceFilterAgent::IsSubresourceFilterChild() {
  return !render_frame()->IsMainFrame() ||
         render_frame()->IsInFencedFrameTree();
}

bool SubresourceFilterAgent::IsParentAdFrame() {
  // A fenced frame root should never ask this since it can't see the outer
  // frame tree. Its AdEvidence is always computed by the browser.
  CHECK(!IsFencedFrameRoot(render_frame()), base::NotFatalUntil::M129);
  return render_frame()->GetWebFrame()->Parent()->IsAdFrame();
}

bool SubresourceFilterAgent::IsProvisional() {
  return render_frame()->GetWebFrame()->IsProvisional();
}

bool SubresourceFilterAgent::IsFrameCreatedByAdScript() {
  CHECK(!IsFencedFrameRoot(render_frame()), base::NotFatalUntil::M129);
  return render_frame()->GetWebFrame()->IsFrameCreatedByAdScript();
}

void SubresourceFilterAgent::SetSubresourceFilterForCurrentDocument(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  CHECK(web_frame->GetDocumentLoader(), base::NotFatalUntil::M129);
  web_frame->GetDocumentLoader()->SetSubresourceFilter(filter.release());
}

void SubresourceFilterAgent::
    SignalFirstSubresourceDisallowedForCurrentDocument() {
  GetSubresourceFilterHost()->DidDisallowFirstSubresource();
}

void SubresourceFilterAgent::SendDocumentLoadStatistics(
    const mojom::DocumentLoadStatistics& statistics) {
  GetSubresourceFilterHost()->SetDocumentLoadStatistics(statistics.Clone());
}

void SubresourceFilterAgent::SendFrameIsAd() {
  GetSubresourceFilterHost()->FrameIsAd();
}

void SubresourceFilterAgent::SendFrameWasCreatedByAdScript() {
  CHECK(!IsFencedFrameRoot(render_frame()), base::NotFatalUntil::M129);
  GetSubresourceFilterHost()->FrameWasCreatedByAdScript();
}

bool SubresourceFilterAgent::IsAdFrame() {
  return render_frame()->GetWebFrame()->IsAdFrame();
}

void SubresourceFilterAgent::SetAdEvidence(
    const blink::FrameAdEvidence& ad_evidence) {
  render_frame()->GetWebFrame()->SetAdEvidence(ad_evidence);
}

const std::optional<blink::FrameAdEvidence>&
SubresourceFilterAgent::AdEvidence() {
  return render_frame()->GetWebFrame()->AdEvidence();
}

// static
mojom::ActivationState SubresourceFilterAgent::GetInheritedActivationState(
    content::RenderFrame* render_frame) {
  if (!render_frame)
    return mojom::ActivationState();

  // A fenced frame is isolated from its outer embedder so we cannot inspect
  // the parent's activation state. However, that's ok because the embedder
  // cannot script the fenced frame so we can wait until a navigation to set
  // activation state.
  if (IsFencedFrameRoot(render_frame))
    return mojom::ActivationState();

  blink::WebFrame* frame_to_inherit_from =
      render_frame->IsMainFrame() ? render_frame->GetWebFrame()->Opener()
                                  : render_frame->GetWebFrame()->Parent();

  if (!frame_to_inherit_from || !frame_to_inherit_from->IsWebLocalFrame())
    return mojom::ActivationState();

  blink::WebSecurityOrigin render_frame_origin =
      render_frame->GetWebFrame()->GetSecurityOrigin();
  blink::WebSecurityOrigin inherited_origin =
      frame_to_inherit_from->GetSecurityOrigin();

  // Only inherit from same-origin frames.
  if (render_frame_origin.IsSameOriginWith(inherited_origin)) {
    auto* agent =
        SubresourceFilterAgent::Get(content::RenderFrame::FromWebFrame(
            frame_to_inherit_from->ToWebLocalFrame()));
    if (agent && agent->filter_for_last_created_document_)
      return agent->filter_for_last_created_document_->activation_state();
  }

  return mojom::ActivationState();
}

void SubresourceFilterAgent::RecordHistogramsOnFilterCreation(
    const mojom::ActivationState& activation_state) {
  // Note: mojom::ActivationLevel used to be called mojom::ActivationState, the
  // legacy name is kept for the histogram.
  mojom::ActivationLevel activation_level = activation_state.activation_level;

  if (!IsSubresourceFilterChild()) {
    UMA_HISTOGRAM_BOOLEAN(
        "SubresourceFilter.MainFrameLoad.RulesetIsAvailableAnyActivationLevel",
        ruleset_dealer_->IsRulesetFileAvailable());
  }
  if (activation_level != mojom::ActivationLevel::kDisabled) {
    UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.DocumentLoad.RulesetIsAvailable",
                          ruleset_dealer_->IsRulesetFileAvailable());
  }
}

void SubresourceFilterAgent::ResetInfoForNextDocument() {
  activation_state_for_next_document_ = mojom::ActivationState();
}

mojom::SubresourceFilterHost*
SubresourceFilterAgent::GetSubresourceFilterHost() {
  if (!subresource_filter_host_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &subresource_filter_host_);
  }
  return subresource_filter_host_.get();
}

void SubresourceFilterAgent::OnSubresourceFilterAgentRequest(
    mojo::PendingAssociatedReceiver<mojom::SubresourceFilterAgent> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void SubresourceFilterAgent::ActivateForNextCommittedLoad(
    mojom::ActivationStatePtr activation_state,
    const std::optional<blink::FrameAdEvidence>& ad_evidence) {
  activation_state_for_next_document_ = *activation_state;
  if (IsSubresourceFilterChild()) {
    CHECK(ad_evidence.has_value(), base::NotFatalUntil::M129);
    SetAdEvidence(ad_evidence.value());
  } else {
    CHECK(!ad_evidence.has_value(), base::NotFatalUntil::M129);
  }
}

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::SetAdEvidenceForInitialEmptySubframe() {
  CHECK(!IsAdFrame(), base::NotFatalUntil::M129);
  CHECK(!AdEvidence().has_value(), base::NotFatalUntil::M129);
  CHECK(!IsFencedFrameRoot(render_frame()), base::NotFatalUntil::M129);

  blink::FrameAdEvidence ad_evidence(IsParentAdFrame());
  ad_evidence.set_created_by_ad_script(
      IsFrameCreatedByAdScript()
          ? blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript
          : blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);
  ad_evidence.set_is_complete();
  SetAdEvidence(ad_evidence);

  if (ad_evidence.IndicatesAdFrame()) {
    SendFrameIsAd();
  }
}

void SubresourceFilterAgent::DidCreateNewDocument() {
  // TODO(csharrison): Use WebURL and WebSecurityOrigin for efficiency here,
  // which requires changes to the unit tests.
  const GURL& url = GetDocumentURL();

  // A new browser-side host is created for each new page (i.e. new document in
  // a subresource filter root frame) so we have to reset the remote so we
  // re-bind on the next message.
  if (!IsSubresourceFilterChild())
    subresource_filter_host_.reset();

  const mojom::ActivationState activation_state =
      ShouldInheritActivation(url) ? GetInheritedActivationStateForNewDocument()
                                   : activation_state_for_next_document_;

  ResetInfoForNextDocument();

  // Do not pollute the histograms with uninteresting root frame documents.
  const bool should_record_histograms = IsSubresourceFilterChild() ||
                                        url.SchemeIsHTTPOrHTTPS() ||
                                        url.SchemeIsFile();
  if (should_record_histograms) {
    RecordHistogramsOnFilterCreation(activation_state);
  }

  ConstructFilter(activation_state, url);
}

const mojom::ActivationState
SubresourceFilterAgent::GetInheritedActivationStateForNewDocument() {
  CHECK(ShouldInheritActivation(GetDocumentURL()), base::NotFatalUntil::M129);
  return GetInheritedActivationState(render_frame());
}

void SubresourceFilterAgent::ConstructFilter(
    const mojom::ActivationState activation_state,
    const GURL& url) {
  filter_for_last_created_document_.reset();

  if (activation_state.activation_level == mojom::ActivationLevel::kDisabled ||
      !ruleset_dealer_->IsRulesetFileAvailable())
    return;

  scoped_refptr<const MemoryMappedRuleset> ruleset =
      ruleset_dealer_->GetRuleset();
  if (!ruleset)
    return;

  base::OnceClosure first_disallowed_load_callback(
      base::BindOnce(&SubresourceFilterAgent::
                         SignalFirstSubresourceDisallowedForCurrentDocument,
                     weak_ptr_factory_.GetWeakPtr()));
  auto filter = std::make_unique<WebDocumentSubresourceFilterImpl>(
      url::Origin::Create(url), activation_state, std::move(ruleset),
      std::move(first_disallowed_load_callback));
  filter_for_last_created_document_ = filter->AsWeakPtr();
  SetSubresourceFilterForCurrentDocument(std::move(filter));
}

void SubresourceFilterAgent::DidFailProvisionalLoad() {
  // TODO(engedy): Add a test with `frame-ancestor` violation to exercise this.
  ResetInfoForNextDocument();
}

void SubresourceFilterAgent::DidFinishLoad() {
  if (!filter_for_last_created_document_)
    return;
  const auto& statistics =
      filter_for_last_created_document_->filter().statistics();
  SendDocumentLoadStatistics(statistics);
}

void SubresourceFilterAgent::WillCreateWorkerFetchContext(
    blink::WebWorkerFetchContext* worker_fetch_context) {
  if (!filter_for_last_created_document_)
    return;
  if (!ruleset_dealer_->IsRulesetFileAvailable())
    return;
  base::File ruleset_file = ruleset_dealer_->DuplicateRulesetFile();
  if (!ruleset_file.IsValid())
    return;

  worker_fetch_context->SetSubresourceFilterBuilder(
      std::make_unique<WebDocumentSubresourceFilterImpl::BuilderImpl>(
          url::Origin::Create(GetDocumentURL()),
          filter_for_last_created_document_->filter().activation_state(),
          std::move(ruleset_file),
          base::BindOnce(&SubresourceFilterAgent::
                             SignalFirstSubresourceDisallowedForCurrentDocument,
                         weak_ptr_factory_.GetWeakPtr())));
}

void SubresourceFilterAgent::OnOverlayPopupAdDetected() {
  GetSubresourceFilterHost()->OnAdsViolationTriggered(
      subresource_filter::mojom::AdsViolation::kOverlayPopupAd);
}

void SubresourceFilterAgent::OnLargeStickyAdDetected() {
  GetSubresourceFilterHost()->OnAdsViolationTriggered(
      subresource_filter::mojom::AdsViolation::kLargeStickyAd);
}

void SubresourceFilterAgent::DidCreateFencedFrame(
    const blink::RemoteFrameToken& placeholder_token) {
  if (render_frame()->GetWebFrame()->IsAdScriptInStack()) {
    GetSubresourceFilterHost()->AdScriptDidCreateFencedFrame(placeholder_token);
  }
}

}  // namespace subresource_filter
