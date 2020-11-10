// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PREVIEWS_RESOURCE_LOADING_HINTS_AGENT_H_
#define CHROME_RENDERER_PREVIEWS_RESOURCE_LOADING_HINTS_AGENT_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/common/subresource_redirect_service.mojom.h"
#include "chrome/renderer/lite_video/lite_video_hint_agent.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "url/gurl.h"

namespace previews {

// The renderer-side agent of ResourceLoadingHintsAgent. There is one instance
// per main frame, responsible for sending the loading hints from browser to
// the document loader.
class ResourceLoadingHintsAgent
    : public content::RenderFrameObserver,
      public blink::mojom::PreviewsResourceLoadingHintsReceiver,
      public base::SupportsWeakPtr<ResourceLoadingHintsAgent>,
      public content::RenderFrameObserverTracker<ResourceLoadingHintsAgent> {
 public:
  ResourceLoadingHintsAgent(
      blink::AssociatedInterfaceRegistry* associated_interfaces,
      content::RenderFrame* render_frame);
  ~ResourceLoadingHintsAgent() override;

  subresource_redirect::SubresourceRedirectHintsAgent&
  subresource_redirect_hints_agent() {
    return subresource_redirect_hints_agent_;
  }

  // Notifies the browser process that https image compression fetch had failed.
  void NotifyHttpsImageCompressionFetchFailed(base::TimeDelta retry_after);

 private:
  // content::RenderFrameObserver:
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidCreateNewDocument() override;
  void OnDestruct() override;

  GURL GetDocumentURL() const;

  // blink::mojom::PreviewsResourceLoadingHintsReceiver:
  void SetResourceLoadingHints(blink::mojom::PreviewsResourceLoadingHintsPtr
                                   resource_loading_hints) override;
  void SetCompressPublicImagesHints(
      blink::mojom::CompressPublicImagesHintsPtr images_hints) override;
  void SetLiteVideoHint(
      blink::mojom::LiteVideoHintPtr lite_video_hint) override;
  void SetBlinkOptimizationGuideHints(
      blink::mojom::BlinkOptimizationGuideHintsPtr hints) override;
  void StopThrottlingMediaRequests() override;

  void SetReceiver(
      mojo::PendingAssociatedReceiver<
          blink::mojom::PreviewsResourceLoadingHintsReceiver> receiver);

  bool IsMainFrame() const;

  std::vector<std::string> subresource_patterns_to_block_;
  base::Optional<int64_t> ukm_source_id_;

  mojo::AssociatedReceiver<blink::mojom::PreviewsResourceLoadingHintsReceiver>
      receiver_{this};

  mojo::AssociatedRemote<
      subresource_redirect::mojom::SubresourceRedirectService>
      subresource_redirect_service_remote_;

  subresource_redirect::SubresourceRedirectHintsAgent
      subresource_redirect_hints_agent_;

  blink::mojom::BlinkOptimizationGuideHintsPtr blink_optimization_guide_hints_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsAgent);
};

}  // namespace previews

#endif  // CHROME_RENDERER_PREVIEWS_RESOURCE_LOADING_HINTS_AGENT_H_
