// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/previews/resource_loading_hints_agent.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace previews {


ResourceLoadingHintsAgent::ResourceLoadingHintsAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  DCHECK(render_frame);
  associated_interfaces->AddInterface(base::BindRepeating(
      &ResourceLoadingHintsAgent::SetReceiver, base::Unretained(this)));
}

GURL ResourceLoadingHintsAgent::GetDocumentURL() const {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

void ResourceLoadingHintsAgent::DidCreateNewDocument() {
  if (!IsMainFrame())
    return;
  if (!GetDocumentURL().SchemeIsHTTPOrHTTPS())
    return;

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK(web_frame);

  // Pass the optimization hints for Blink to LocalFrame.
  // TODO(https://crbug.com/1113980): Onion-soupify the optimization guide for
  // Blink so that we can directly pass the hints without mojom variant
  // conversion.
  if (blink_optimization_guide_hints_) {
    blink::WebOptimizationGuideHints hints;
    if (blink_optimization_guide_hints_->delay_async_script_execution_hints) {
      hints.delay_async_script_execution_delay_type =
          blink_optimization_guide_hints_->delay_async_script_execution_hints
              ->delay_type;
    }
    if (blink_optimization_guide_hints_
            ->delay_competing_low_priority_requests_hints) {
      hints.delay_competing_low_priority_requests_delay_type =
          blink_optimization_guide_hints_
              ->delay_competing_low_priority_requests_hints->delay_type;
      hints.delay_competing_low_priority_requests_priority_threshold =
          blink_optimization_guide_hints_
              ->delay_competing_low_priority_requests_hints->priority_threshold;
    }
    web_frame->SetOptimizationGuideHints(hints);
  }
  // Once the hints are sent to the local frame, clear the local copy to prevent
  // accidental reuse.
  blink_optimization_guide_hints_.reset();
}

void ResourceLoadingHintsAgent::OnDestruct() {
  delete this;
}

ResourceLoadingHintsAgent::~ResourceLoadingHintsAgent() = default;

void ResourceLoadingHintsAgent::SetReceiver(
    mojo::PendingAssociatedReceiver<
        previews::mojom::PreviewsResourceLoadingHintsReceiver> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

bool ResourceLoadingHintsAgent::IsMainFrame() const {
  return render_frame()->IsMainFrame();
}


void ResourceLoadingHintsAgent::SetLiteVideoHint(
    previews::mojom::LiteVideoHintPtr lite_video_hint) {
  auto* lite_video_hint_agent =
      lite_video::LiteVideoHintAgent::Get(render_frame());
  if (lite_video_hint_agent)
    lite_video_hint_agent->SetLiteVideoHint(std::move(lite_video_hint));
}

void ResourceLoadingHintsAgent::SetBlinkOptimizationGuideHints(
    blink::mojom::BlinkOptimizationGuideHintsPtr hints) {
  if (!IsMainFrame())
    return;
  blink_optimization_guide_hints_ = std::move(hints);
}

void ResourceLoadingHintsAgent::StopThrottlingMediaRequests() {
  auto* lite_video_hint_agent =
      lite_video::LiteVideoHintAgent::Get(render_frame());
  if (lite_video_hint_agent) {
    LOCAL_HISTOGRAM_BOOLEAN("LiteVideo.HintsAgent.StopThrottling", true);
    lite_video_hint_agent->StopThrottlingAndClearHints();
  }
}

}  // namespace previews
