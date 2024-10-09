// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/fingerprinting_protection_filter/renderer/unverified_ruleset_dealer.h"
#include "components/subresource_filter/content/shared/common/utils.h"
#include "components/subresource_filter/core/common/document_subresource_filter.h"
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

GURL RendererAgent::GetMainDocumentUrl() {
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
  return render_frame()->IsMainFrame() &&
         !render_frame()->IsInFencedFrameTree();
}

bool RendererAgent::HasValidOpener() {
  return render_frame()->GetWebFrame()->Opener() &&
         render_frame()->GetWebFrame()->Opener()->IsWebLocalFrame();
}

RendererAgent::RendererAgent(content::RenderFrame* render_frame,
                             UnverifiedRulesetDealer* ruleset_dealer)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<RendererAgent>(render_frame),
      ruleset_dealer_(ruleset_dealer) {}

RendererAgent::~RendererAgent() = default;

std::optional<subresource_filter::mojom::ActivationState>
RendererAgent::GetInheritedActivationState() {
  // A fenced frame is isolated from its outer embedder so we cannot inspect
  // the parent's activation state. However, that's ok because the embedder
  // cannot script the fenced frame so we can wait until a navigation to set
  // activation state.
  if (IsFencedFrameRoot(render_frame())) {
    return std::nullopt;
  }

  blink::WebFrame* frame_to_inherit_from =
      render_frame()->IsMainFrame() ? render_frame()->GetWebFrame()->Opener()
                                    : render_frame()->GetWebFrame()->Parent();

  if (!frame_to_inherit_from || !frame_to_inherit_from->IsWebLocalFrame()) {
    return std::nullopt;
  }

  blink::WebSecurityOrigin render_frame_origin =
      render_frame()->GetWebFrame()->GetSecurityOrigin();
  blink::WebSecurityOrigin inherited_origin =
      frame_to_inherit_from->GetSecurityOrigin();

  // Only inherit from same-origin frames, or any origin if the current frame
  // doesn't have one.
  if (render_frame_origin.IsNull() ||
      render_frame_origin.IsSameOriginWith(inherited_origin)) {
    auto* agent = RendererAgent::Get(content::RenderFrame::FromWebFrame(
        frame_to_inherit_from->ToWebLocalFrame()));
    if (agent) {
      return agent->filter_ ? agent->filter_->activation_state()
                            : subresource_filter::mojom::ActivationState();
    }
  }
  return std::nullopt;
}

void RendererAgent::RequestActivationState() {
  CHECK(pending_activation_);
  activation_state_ = subresource_filter::mojom::ActivationState();
  // We will be notified of activation with a callback if there is a valid
  // `FingerprintingProtectionHost` on the browser.
  auto* fp_host = GetFingerprintingProtectionHost();
  if (fp_host) {
    fp_host->CheckActivation(base::BindOnce(
        &RendererAgent::OnActivationComputed, base::Unretained(this)));
  }
}

void RendererAgent::Initialize() {
  current_document_url_ = GetMainDocumentUrl();
  pending_activation_ = true;

  if (!IsTopLevelMainFrame() || HasValidOpener()) {
    // Attempt to inherit activation only for child frames or main frames that
    // are opened from another page.
    std::optional<subresource_filter::mojom::ActivationState> inherited_state =
        GetInheritedActivationState();
    if (inherited_state.has_value()) {
      activation_state_ = inherited_state.value();
      pending_activation_ = false;
      bool activation_enabled =
          activation_state_.activation_level !=
          subresource_filter::mojom::ActivationLevel::kDisabled;
      if (activation_enabled) {
        MaybeCreateNewFilter();
      }
    }
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
    notified_disallow_ = false;
    auto new_origin = url::Origin::Create(new_document_url);
    auto current_origin = url::Origin::Create(current_document_url_);
    // Could be same origin for refreshes, etc.
    if (!new_origin.IsSameOriginWith(current_origin)) {
      filter_.reset();
      Initialize();
    }
  }
  current_document_url_ = new_document_url;
}

void RendererAgent::DidFailProvisionalLoad() {
  // We know the document will change (or this agent will be deleted) since a
  // navigation did not commit - set up to request new activation in
  // `DidCreateNewDocument()`.
  activation_state_ = subresource_filter::mojom::ActivationState();
  pending_activation_ = true;
}

void RendererAgent::DidFinishLoad() {
  if (!filter_) {
    return;
  }
  const auto& statistics = filter_->statistics();
  SendDocumentLoadStatistics(statistics);
}

void RendererAgent::OnDestruct() {
  // Deleting itself here ensures that a `RendererAgent` does not need to
  // check the validity of `render_frame()` before using it and avoids a memory
  // leak.
  delete this;
}

void RendererAgent::OnSubresourceDisallowed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Notify the browser that a subresource was disallowed on the renderer
  // (for metrics or UI logic).
  if (!notified_disallow_) {
    notified_disallow_ = true;
    auto* fp_host = GetFingerprintingProtectionHost();
    if (fp_host) {
      fp_host->DidDisallowFirstSubresource();
    }
  }
}

void RendererAgent::OnActivationComputed(
    subresource_filter::mojom::ActivationStatePtr activation_state) {
  if (!pending_activation_) {
    return;
  }

  activation_state_ = *activation_state;
  pending_activation_ = false;

  if (activation_state_.activation_level !=
      subresource_filter::mojom::ActivationLevel::kDisabled) {
    MaybeCreateNewFilter();
  }

  for (auto& callback : pending_activation_callbacks_) {
    std::move(callback).Run(activation_state_);
  }
  pending_activation_callbacks_.clear();
}

void RendererAgent::GetActivationState(ActivationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_activation_) {
    std::move(callback).Run(activation_state_);
  } else {
    pending_activation_callbacks_.emplace_back(std::move(callback));
  }
}

void RendererAgent::CheckURL(const GURL& url,
                             url_pattern_index::proto::ElementType element_type,
                             FilterCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subresource_filter::LoadPolicy load_policy =
      subresource_filter::LoadPolicy::ALLOW;
  if (filter_) {
    load_policy = filter_->GetLoadPolicy(url, element_type);
  }
  std::move(callback).Run(load_policy);
}

mojom::FingerprintingProtectionHost*
RendererAgent::GetFingerprintingProtectionHost() {
  if (!fingerprinting_protection_host_.is_bound()) {
    // Attempt a new connection to a host on the browser.
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &fingerprinting_protection_host_);
    // Default to disabled activation if there is no response from the browser
    // before the host disconnects. This handler will not be called if the
    // host is reset due to a new document being created on the same frame.
    fingerprinting_protection_host_.set_disconnect_handler(base::BindOnce(
        &RendererAgent::OnActivationComputed, base::Unretained(this),
        subresource_filter::mojom::ActivationState::New()));
  }
  return fingerprinting_protection_host_.is_bound()
             ? fingerprinting_protection_host_.get()
             : nullptr;
}

void RendererAgent::SetFilter(
    std::unique_ptr<subresource_filter::DocumentSubresourceFilter> filter) {
  filter_ = std::move(filter);
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

  url::Origin origin = url::Origin::Create(current_document_url_);
  SetFilter(std::make_unique<subresource_filter::DocumentSubresourceFilter>(
      std::move(origin), activation_state_, std::move(ruleset)));
}

void RendererAgent::SendDocumentLoadStatistics(
    const subresource_filter::mojom::DocumentLoadStatistics& statistics) {
  GetFingerprintingProtectionHost()->SetDocumentLoadStatistics(
      statistics.Clone());
}

}  // namespace fingerprinting_protection_filter
