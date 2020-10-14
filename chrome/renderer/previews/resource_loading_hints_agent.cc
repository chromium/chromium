// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/previews/resource_loading_hints_agent.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_loading_hints_provider.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace previews {

namespace {

const blink::WebVector<blink::WebString> convert_to_web_vector(
    const std::vector<std::string>& subresource_patterns_to_block) {
  blink::WebVector<blink::WebString> web_vector(
      subresource_patterns_to_block.size());
  for (const std::string& element : subresource_patterns_to_block) {
    web_vector.emplace_back(blink::WebString::FromASCII(element));
  }
  return web_vector;
}

}  // namespace

ResourceLoadingHintsAgent::ResourceLoadingHintsAgent(
    blink::AssociatedInterfaceRegistry* associated_interfaces,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ResourceLoadingHintsAgent>(
          render_frame) {
  DCHECK(render_frame);
  associated_interfaces->AddInterface(base::BindRepeating(
      &ResourceLoadingHintsAgent::SetReceiver, base::Unretained(this)));
}

GURL ResourceLoadingHintsAgent::GetDocumentURL() const {
  return render_frame()->GetWebFrame()->GetDocument().Url();
}

void ResourceLoadingHintsAgent::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  if (!IsMainFrame())
    return;
  subresource_redirect_hints_agent_.DidStartNavigation();
}

void ResourceLoadingHintsAgent::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  if (!IsMainFrame())
    return;

  subresource_redirect_hints_agent_.ReadyToCommitNavigation(
      render_frame()->GetRoutingID());
}

void ResourceLoadingHintsAgent::DidCreateNewDocument() {
  if (!IsMainFrame())
    return;
  if (!GetDocumentURL().SchemeIsHTTPOrHTTPS())
    return;

  blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
  DCHECK(web_frame);

  if (!subresource_patterns_to_block_.empty()) {
    std::unique_ptr<blink::WebLoadingHintsProvider> loading_hints =
        std::make_unique<blink::WebLoadingHintsProvider>(
            ukm_source_id_.value(),
            convert_to_web_vector(subresource_patterns_to_block_));

    web_frame->GetDocumentLoader()->SetLoadingHintsProvider(
        std::move(loading_hints));
    // Once the hints are sent to the document loader, clear the local copy to
    // prevent accidental reuse.
    subresource_patterns_to_block_.clear();
  }

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
        blink::mojom::PreviewsResourceLoadingHintsReceiver> receiver) {
  receiver_.Bind(std::move(receiver));
}

bool ResourceLoadingHintsAgent::IsMainFrame() const {
  return render_frame()->IsMainFrame();
}

void ResourceLoadingHintsAgent::SetResourceLoadingHints(
    blink::mojom::PreviewsResourceLoadingHintsPtr resource_loading_hints) {
  if (!IsMainFrame())
    return;

  ukm_source_id_ = resource_loading_hints->ukm_source_id;

  for (const auto& subresource :
       resource_loading_hints->subresources_to_block) {
    subresource_patterns_to_block_.push_back(subresource);
  }
}

void ResourceLoadingHintsAgent::SetCompressPublicImagesHints(
    blink::mojom::CompressPublicImagesHintsPtr images_hints) {
  if (!IsMainFrame())
    return;
  subresource_redirect_hints_agent_.SetCompressPublicImagesHints(
      std::move(images_hints));
}

void ResourceLoadingHintsAgent::NotifyHttpsImageCompressionFetchFailed(
    base::TimeDelta retry_after) {
  if (!subresource_redirect_service_remote_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &subresource_redirect_service_remote_);
  }
  subresource_redirect_service_remote_->NotifyCompressedImageFetchFailed(
      retry_after);
}

void ResourceLoadingHintsAgent::SetLiteVideoHint(
    blink::mojom::LiteVideoHintPtr lite_video_hint) {
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
