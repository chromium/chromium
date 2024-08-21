// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/fingerprinting_protection_filter/renderer/renderer_url_loader_throttle.h"
#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/shared/common/subresource_filter_utils.h"
#include "components/subresource_filter/content/shared/renderer/web_document_subresource_filter_impl.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {
namespace {

// Returns whether a `RenderFrame` is the root of a fenced frame tree within
// another frame tree.
bool IsFencedFrameRoot(content::RenderFrame* render_frame) {
  // Unit tests may have a nullptr render_frame.
  if (!render_frame) {
    return false;
  }
  return render_frame->IsInFencedFrameTree() && render_frame->IsMainFrame();
}

}  // namespace

// static
//
// If no activation state is found to inherit the returned `ActivationLevel`
// will be `kDisabled`.
subresource_filter::mojom::ActivationState
RendererAgent::GetInheritedActivationState(content::RenderFrame* render_frame) {
  if (!render_frame) {
    return subresource_filter::mojom::ActivationState();
  }

  // A fenced frame is isolated from its outer embedder so we cannot inspect
  // the parent's activation state. However, that's ok because the embedder
  // cannot script the fenced frame so we can wait until a navigation to set
  // activation state.
  if (IsFencedFrameRoot(render_frame)) {
    return subresource_filter::mojom::ActivationState();
  }

  blink::WebFrame* frame_to_inherit_from =
      render_frame->IsMainFrame() ? render_frame->GetWebFrame()->Opener()
                                  : render_frame->GetWebFrame()->Parent();

  if (!frame_to_inherit_from || !frame_to_inherit_from->IsWebLocalFrame()) {
    return subresource_filter::mojom::ActivationState();
  }

  blink::WebSecurityOrigin render_frame_origin =
      render_frame->GetWebFrame()->GetSecurityOrigin();
  blink::WebSecurityOrigin inherited_origin =
      frame_to_inherit_from->GetSecurityOrigin();

  // Only inherit from same-origin frames.
  if (render_frame_origin.IsSameOriginWith(inherited_origin)) {
    auto* agent = RendererAgent::Get(content::RenderFrame::FromWebFrame(
        frame_to_inherit_from->ToWebLocalFrame()));
    if (agent && agent->filter_) {
      return agent->filter_->activation_state();
    }
  }

  return subresource_filter::mojom::ActivationState();
}

GURL RendererAgent::GetMainDocumentUrl() {
  if (!render_frame()) {
    return GURL();
  }
  auto* main_render_frame = render_frame()->IsMainFrame()
                                ? render_frame()
                                : render_frame()->GetMainRenderFrame();
  if (!main_render_frame || !main_render_frame->GetWebFrame()) {
    return GURL();
  }
  GURL url = main_render_frame->GetWebFrame()->GetDocument().Url();
  return url.SchemeIsHTTPOrHTTPS() ? url : GURL();
}

bool RendererAgent::IsTopLevelMainFrame() {
  if (!render_frame()) {
    return false;
  }
  return render_frame()->IsMainFrame() &&
         !render_frame()->IsInFencedFrameTree();
}

RendererAgent::RendererAgent(content::RenderFrame* render_frame,
                             UnverifiedRulesetDealer* ruleset_dealer)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<RendererAgent>(render_frame),
      ruleset_dealer_(ruleset_dealer) {}

RendererAgent::~RendererAgent() {
  DeleteAllThrottles();
}

void RendererAgent::RequestActivationState() {
  if (render_frame()) {
    // We will be notified of activation with a callback if there is a valid
    // `FingerprintingProtectionHost` on the browser.
    GetFingerprintingProtectionHost()->CheckActivation(base::BindOnce(
        &RendererAgent::OnActivationComputed, base::Unretained(this)));
  }
}

void RendererAgent::Initialize() {
  current_document_url_ = GetMainDocumentUrl();
  pending_activation_ = true;

  if (subresource_filter::ShouldInheritActivation(current_document_url_)) {
    activation_state_ = GetInheritedActivationState(render_frame());
    pending_activation_ =
        (activation_state_.activation_level ==
         subresource_filter::mojom::ActivationLevel::kDisabled);
    MaybeCreateNewFilter();
  }
  if (pending_activation_) {
    RequestActivationState();
  }
}

void RendererAgent::DidCreateNewDocument() {
  GURL new_document_url = GetMainDocumentUrl();

  // A new browser-side host is created for each new page (i.e. new document in
  // a root frame) so we have to reset the remote so we re-bind on the next
  // message.
  if (IsTopLevelMainFrame()) {
    fingerprinting_protection_host_.reset();
    auto new_origin = url::Origin::Create(new_document_url);
    auto current_origin = url::Origin::Create(current_document_url_);
    // Could be same origin for refreshes, etc.
    if (!new_origin.IsSameOriginWith(current_origin)) {
      filter_.reset();
    }
  }

  current_document_url_ = new_document_url;
  if (current_document_url_ != GURL()) {
    // The main document for the page has changed - request new activation.
    RequestActivationState();
  }
}

void RendererAgent::DidFailProvisionalLoad() {
  // We know the document will change (or this agent will be deleted) since a
  // navigation did not commit - set up to request new activation.
  activation_state_ = subresource_filter::mojom::ActivationState();
  pending_activation_ = true;
}

void RendererAgent::WillDetach(blink::DetachReason detach_reason) {
  DeleteAllThrottles();
}

void RendererAgent::OnDestruct() {
  DeleteAllThrottles();
}

void RendererAgent::AddThrottle(RendererURLLoaderThrottle* throttle) {
  // Should only be called by throttles, so the throttle should always be valid.
  CHECK(throttle);
  throttles_.insert(throttle);
  if (!pending_activation_) {
    // Notify the new throttle if we already know the activation state.
    throttle->OnActivationComputed(activation_state_);
  }
}

void RendererAgent::DeleteThrottle(RendererURLLoaderThrottle* throttle) {
  if (!throttle) {
    return;
  }
  throttles_.erase(throttle);
}

void RendererAgent::DeleteAllThrottles() {
  for (const raw_ptr<RendererURLLoaderThrottle> throttle : throttles_) {
    if (throttle) {
      // Notify throttles of activation so any ongoing loads are not left
      // deferred.
      throttle->OnActivationComputed(activation_state_);
    }
  }

  for (raw_ptr<RendererURLLoaderThrottle>& throttle : throttles_) {
    DeleteThrottle(throttle);
  }
}

void RendererAgent::OnSubresourceDisallowed() {
  // Notify the browser that a subresource was disallowed on the renderer
  // (for metrics or UI logic).
  GetFingerprintingProtectionHost()->DidDisallowFirstSubresource();
}

void RendererAgent::OnActivationComputed(
    subresource_filter::mojom::ActivationStatePtr activation_state) {
  activation_state_ = *activation_state;
  pending_activation_ = false;

  if (activation_state_.activation_level !=
      subresource_filter::mojom::ActivationLevel::kDisabled) {
    MaybeCreateNewFilter();
  }

  for (const raw_ptr<RendererURLLoaderThrottle> throttle : throttles_) {
    throttle->OnActivationComputed(*activation_state);
  }
}

mojom::FingerprintingProtectionHost*
RendererAgent::GetFingerprintingProtectionHost() {
  if (!fingerprinting_protection_host_) {
    if (render_frame()) {
      // Attempt a new connection to a host on the browser.
      render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
          &fingerprinting_protection_host_);
    }
  }
  return fingerprinting_protection_host_.get();
}

void RendererAgent::SetFilterForCurrentDocument(
    std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) {
  if (!render_frame()) {
    return;
  }
  CHECK(render_frame()->GetWebFrame());
  CHECK(render_frame()->GetWebFrame()->GetDocumentLoader());

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  CHECK(web_frame->GetDocumentLoader());
  // TODO(https://crbug.com/40280666): Set the filter on the `DocumentLoader`
  // once it is supported.
}

void RendererAgent::MaybeCreateNewFilter() {
  if (pending_activation_ ||
      activation_state_.activation_level ==
          subresource_filter::mojom::ActivationLevel::kDisabled) {
    return;
  }

  if (!ruleset_dealer_ || !ruleset_dealer_->IsRulesetFileAvailable()) {
    return;
  }

  scoped_refptr<const subresource_filter::MemoryMappedRuleset> ruleset =
      ruleset_dealer_->GetRuleset();
  if (!ruleset) {
    return;
  }

  if (current_document_url_ == GURL()) {
    // There is no valid document to filter.
    return;
  }

  base::OnceClosure first_disallowed_load_callback(base::BindOnce(
      &RendererAgent::OnSubresourceDisallowed, weak_factory_.GetWeakPtr()));
  url::Origin origin = url::Origin::Create(current_document_url_);
  auto new_filter =
      std::make_unique<subresource_filter::WebDocumentSubresourceFilterImpl>(
          std::move(origin), activation_state_, std::move(ruleset),
          std::move(first_disallowed_load_callback));
  filter_ = new_filter->AsWeakPtr();
  SetFilterForCurrentDocument(std::move(new_filter));
}

}  // namespace fingerprinting_protection_filter
