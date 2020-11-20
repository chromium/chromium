// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/common/subresource_filter_utils.h"
#include "components/subresource_filter/content/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/renderer/web_document_subresource_filter_impl.h"
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
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/url_constants.h"

namespace subresource_filter {

SubresourceFilterAgent::SubresourceFilterAgent(
    content::RenderFrame* render_frame,
    UnverifiedRulesetDealer* ruleset_dealer,
    std::unique_ptr<AdResourceTracker> ad_resource_tracker)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SubresourceFilterAgent>(render_frame),
      ruleset_dealer_(ruleset_dealer),
      ad_resource_tracker_(std::move(ad_resource_tracker)) {
  DCHECK(ruleset_dealer);
  // |render_frame| can be nullptr in unit tests.
  if (render_frame) {
    // If a mainframe has an activated opener, we activate the initial empty
    // document, which is created before this constructor. This ensures that a
    // popup's final document is appropriately activated, even when the the
    // initial navigation is aborted and there are no further documents created.
    if (render_frame->IsMainFrame() &&
        GetInheritedActivationState(render_frame).activation_level !=
            mojom::ActivationLevel::kDisabled) {
      const GURL& url = GetDocumentURL();
      DCHECK(url.is_empty());
      DCHECK(ShouldInheritActivation(url));
      ConstructFilter(GetInheritedActivationState(render_frame), url);
    }
    render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
        base::BindRepeating(
            &SubresourceFilterAgent::OnSubresourceFilterAgentRequest,
            base::Unretained(this)));
  }
}

SubresourceFilterAgent::~SubresourceFilterAgent() {
  // Filter may outlive us, so reset the ad tracker.
  if (filter_for_last_created_document_)
    filter_for_last_created_document_->set_ad_resource_tracker(nullptr);
}

GURL SubresourceFilterAgent::GetDocumentURL() {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

bool SubresourceFilterAgent::IsMainFrame() {
  return render_frame()->IsMainFrame();
}

bool SubresourceFilterAgent::HasDocumentLoader() {
  return render_frame()->GetWebFrame()->GetDocumentLoader();
}

void SubresourceFilterAgent::SetSubresourceFilterForCurrentDocument(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK(web_frame->GetDocumentLoader());
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

void SubresourceFilterAgent::SendFrameIsAdSubframe() {
  GetSubresourceFilterHost()->FrameIsAdSubframe();
}

bool SubresourceFilterAgent::IsAdSubframe() {
  return render_frame()->GetWebFrame()->IsAdSubframe();
}

void SubresourceFilterAgent::SetIsAdSubframe(
    blink::mojom::AdFrameType ad_frame_type) {
  render_frame()->GetWebFrame()->SetIsAdSubframe(ad_frame_type);
}

// static
mojom::ActivationState SubresourceFilterAgent::GetInheritedActivationState(
    content::RenderFrame* render_frame) {
  if (!render_frame)
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
  UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.DocumentLoad.ActivationState",
                            activation_level);

  if (IsMainFrame()) {
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
  receiver_.Bind(std::move(receiver));
}

void SubresourceFilterAgent::ActivateForNextCommittedLoad(
    mojom::ActivationStatePtr activation_state,
    blink::mojom::AdFrameType ad_frame_type) {
  activation_state_for_next_document_ = *activation_state;
  if (ad_frame_type != blink::mojom::AdFrameType::kNonAd) {
    SetIsAdSubframe(ad_frame_type);
  }
}

void SubresourceFilterAgent::OnDestruct() {
  delete this;
}

void SubresourceFilterAgent::DidCreateNewDocument() {
  // TODO(csharrison): Use WebURL and WebSecurityOrigin for efficiency here,
  // which requires changes to the unit tests.
  const GURL& url = GetDocumentURL();
  // Do not pollute the histograms with the empty main frame documents and
  // initial empty documents, even though some will not have a second document.
  const bool should_record_histograms =
      !first_document_ &&
      !(IsMainFrame() && !url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsFile());
  if (first_document_ && !IsMainFrame()) {
    DCHECK(!filter_for_last_created_document_);

    // Local subframes will first create an initial empty document (with url
    // kAboutBlankURL) no matter what the src is set to (or if it is not set).
    // Then, if the src is set (including to kAboutBlankURL), there is a second
    // document created with url equal to the set src. Due to a bug, currently
    // a second document (with url kAboutBlankURL) is created even if the src is
    // unset (see crbug.com/778318), but we should not rely on this erroneous
    // behavior. Frames created by the browser initialize the LocalFrame before
    // creating RenderFrameObservers, so the initial empty document isn't
    // observed. We only care about local subframes.
    if (url == url::kAboutBlankURL) {
      if (IsAdSubframe())
        SendFrameIsAdSubframe();
    }
  }
  first_document_ = false;

  const mojom::ActivationState activation_state =
      ShouldInheritActivation(url) ? GetInheritedActivationStateForNewDocument()
                                   : activation_state_for_next_document_;

  ResetInfoForNextDocument();

  if (should_record_histograms) {
    RecordHistogramsOnFilterCreation(activation_state);
  }

  ConstructFilter(activation_state, url);
}

const mojom::ActivationState
SubresourceFilterAgent::GetInheritedActivationStateForNewDocument() {
  DCHECK(ShouldInheritActivation(GetDocumentURL()));
  return GetInheritedActivationState(render_frame());
}

void SubresourceFilterAgent::ConstructFilter(
    const mojom::ActivationState activation_state,
    const GURL& url) {
  // Filter may outlive us, so reset the ad tracker.
  if (filter_for_last_created_document_)
    filter_for_last_created_document_->set_ad_resource_tracker(nullptr);
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
                     AsWeakPtr()));
  auto filter = std::make_unique<WebDocumentSubresourceFilterImpl>(
      url::Origin::Create(url), activation_state, std::move(ruleset),
      std::move(first_disallowed_load_callback));
  filter->set_ad_resource_tracker(ad_resource_tracker_.get());
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
                         AsWeakPtr())));
}

}  // namespace subresource_filter
