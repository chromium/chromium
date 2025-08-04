// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/renderer/renderer_agent.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/types/optional_ref.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/mojom/fingerprinting_protection_filter.mojom.h"
#include "components/subresource_filter/content/shared/common/utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"
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

RendererAgent::RendererAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<RendererAgent>(render_frame) {}

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
    if (agent && agent->activation_state_to_inherit_.has_value()) {
      return agent->activation_state_to_inherit_.value();
    }
  }
  return std::nullopt;
}

void RendererAgent::Initialize() {
  current_document_url_ = GetMainDocumentUrl();
  pending_activation_ = true;

  // Null in unit tests.
  if (render_frame()) {
    render_frame()
        ->GetAssociatedInterfaceRegistry()
        ->AddInterface<mojom::FingerprintingProtectionAgent>(
            base::BindRepeating(
                &RendererAgent::OnFingerprintingProtectionAgentRequest,
                base::Unretained(this)));
  }

  if (!IsTopLevelMainFrame() || HasValidOpener()) {
    // Attempt to inherit activation only for child frames or main frames that
    // are opened from another page.
    std::optional<subresource_filter::mojom::ActivationState> inherited_state =
        GetInheritedActivationState();
    if (inherited_state.has_value()) {
      activation_state_for_next_document_ = inherited_state.value();
      pending_activation_ = false;
      MaybeSendActivationToThrottles();
    }
  }
}

void RendererAgent::DidCreateNewDocument() {
  GURL new_document_url = GetMainDocumentUrl();

  if (IsTopLevelMainFrame()) {
    // A new browser-side host is created for each new page (i.e. new document
    // in a root frame) so we have to reset the remote so we re-bind on the next
    // message.
    fingerprinting_protection_host_.reset();
    notified_disallow_ = false;
  }
  current_document_url_ = new_document_url;
  auto inherited_activation_state = GetInheritedActivationState();
  activation_state_for_next_document_ =
      inherited_activation_state.has_value()
          ? inherited_activation_state.value()
          : activation_state_for_next_document_;
  pending_activation_ = false;

  MaybeSendActivationToThrottles();
}

void RendererAgent::DidFailProvisionalLoad() {
  // Reset activation in preparation for receiving a new signal from the browser
  // since a navigation did not commit. This may or may or not result in
  // creating a new document, particularly for downloads.
  activation_state_for_next_document_ =
      subresource_filter::mojom::ActivationState();
  pending_activation_ = true;
}

void RendererAgent::DidFinishLoad() {
  SendDocumentLoadStatistics(aggregated_document_statistics_);
  aggregated_document_statistics_ =
      subresource_filter::mojom::DocumentLoadStatistics();
}

void RendererAgent::OnDestruct() {
  // Deleting itself here ensures that a `RendererAgent` does not need to
  // check the validity of `render_frame()` before using it and avoids a memory
  // leak.
  delete this;
}

RendererAgent::OnSubresourceEvaluatedCallback
RendererAgent::GetOnSubresourceCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindPostTaskToCurrentDefault(base::BindRepeating(
      &RendererAgent::OnSubresourceEvaluated, weak_factory_.GetWeakPtr()));
}

void RendererAgent::OnSubresourceEvaluated(
    const GURL& url,
    const std::optional<std::string>& devtools_request_id,
    bool subresource_disallowed,
    const subresource_filter::mojom::DocumentLoadStatistics& statistics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (subresource_disallowed) {
    OnSubresourceDisallowed(url, devtools_request_id);
  }
  OnSubresourceEvaluatedImpl(statistics);
}

void RendererAgent::OnSubresourceEvaluatedImpl(
    const subresource_filter::mojom::DocumentLoadStatistics& statistics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Note: Chances of overflow are negligible.
  aggregated_document_statistics_.num_loads_total += statistics.num_loads_total;
  aggregated_document_statistics_.num_loads_evaluated +=
      statistics.num_loads_evaluated;
  aggregated_document_statistics_.num_loads_matching_rules +=
      statistics.num_loads_matching_rules;
  aggregated_document_statistics_.num_loads_disallowed +=
      statistics.num_loads_disallowed;

  aggregated_document_statistics_.evaluation_total_wall_duration +=
      statistics.evaluation_total_wall_duration;
  aggregated_document_statistics_.evaluation_total_cpu_duration +=
      statistics.evaluation_total_cpu_duration;
}

void RendererAgent::OnSubresourceDisallowed(
    const GURL& url,
    const std::optional<std::string>& devtools_request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  render_frame()->GetWebFrame()->AddUserReidentificationIssue(
      devtools_request_id, url);
  if (!notified_disallow_) {
    notified_disallow_ = true;

    // Notify the browser that a subresource was disallowed on the renderer
    // (for metrics or UI logic).
    auto* fp_host = GetFingerprintingProtectionHost();
    if (fp_host) {
      fp_host->DidDisallowFirstSubresource();
    }
  }
}

void RendererAgent::ActivateForNextCommittedLoad(
    subresource_filter::mojom::ActivationStatePtr activation_state) {
  activation_state_for_next_document_ = *activation_state;
  pending_activation_ = false;
}

void RendererAgent::SendActivationToAllPendingThrottles() {
  for (auto& activation_computed_callback : activation_computed_callbacks_) {
    std::move(activation_computed_callback)
        .Run(activation_state_to_inherit_.has_value()
                 ? activation_state_to_inherit_.value()
                 : activation_state_for_next_document_,
             GetOnSubresourceCallback(), current_document_url_);
  }
  activation_computed_callbacks_.clear();
}

void RendererAgent::AddActivationComputedCallback(
    ActivationComputedCallback activation_computed_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_activation_) {
    // The call to this function arrives asynchronously so the `RendererAgent`'s
    // various possible internal states of tracking activation need to be
    // considered. If `DidCreateNewDocument` has not yet been called since
    // getting activation from the browser, activation_state_for_next_document_
    // is the most up-to-date activation state.
    std::move(activation_computed_callback)
        .Run(activation_state_to_inherit_.has_value()
                 ? activation_state_to_inherit_.value()
                 : activation_state_for_next_document_,
             GetOnSubresourceCallback(), current_document_url_);

    return;
  }

  // If activation state has not yet arrived from the browser, we keep track of
  // the throttle to notify it of activation later.
  activation_computed_callbacks_.push_back(
      std::move(activation_computed_callback));
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
        &RendererAgent::ActivateForNextCommittedLoad, base::Unretained(this),
        subresource_filter::mojom::ActivationState::New()));
  }
  return fingerprinting_protection_host_.is_bound()
             ? fingerprinting_protection_host_.get()
             : nullptr;
}

void RendererAgent::OnFingerprintingProtectionAgentRequest(
    mojo::PendingAssociatedReceiver<mojom::FingerprintingProtectionAgent>
        receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void RendererAgent::MaybeSendActivationToThrottles() {
  if (pending_activation_ || current_document_url_ == GURL()) {
    // There is either no activation or no valid document to filter.
    return;
  }

  activation_state_to_inherit_ = activation_state_for_next_document_;
  SendActivationToAllPendingThrottles();
  activation_state_for_next_document_ =
      subresource_filter::mojom::ActivationState();
}

void RendererAgent::SendDocumentLoadStatistics(
    const subresource_filter::mojom::DocumentLoadStatistics& statistics) {
  GetFingerprintingProtectionHost()->SetDocumentLoadStatistics(
      statistics.Clone());
}

}  // namespace fingerprinting_protection_filter
