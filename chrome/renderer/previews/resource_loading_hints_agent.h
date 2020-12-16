// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PREVIEWS_RESOURCE_LOADING_HINTS_AGENT_H_
#define CHROME_RENDERER_PREVIEWS_RESOURCE_LOADING_HINTS_AGENT_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/common/previews_resource_loading_hints.mojom.h"
#include "chrome/renderer/lite_video/lite_video_hint_agent.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "url/gurl.h"

namespace previews {

// The renderer-side agent of ResourceLoadingHintsAgent. There is one instance
// per main frame, responsible for sending the loading hints from browser to
// the document loader.
class ResourceLoadingHintsAgent
    : public content::RenderFrameObserver,
      public previews::mojom::PreviewsResourceLoadingHintsReceiver,
      public base::SupportsWeakPtr<ResourceLoadingHintsAgent> {
 public:
  ResourceLoadingHintsAgent(
      blink::AssociatedInterfaceRegistry* associated_interfaces,
      content::RenderFrame* render_frame);
  ~ResourceLoadingHintsAgent() override;

 private:
  // content::RenderFrameObserver:
  void DidCreateNewDocument() override;
  void OnDestruct() override;

  GURL GetDocumentURL() const;

  // previews::mojom::PreviewsResourceLoadingHintsReceiver:
  void SetLiteVideoHint(
      previews::mojom::LiteVideoHintPtr lite_video_hint) override;
  void SetBlinkOptimizationGuideHints(
      blink::mojom::BlinkOptimizationGuideHintsPtr hints) override;
  void StopThrottlingMediaRequests() override;

  void SetReceiver(
      mojo::PendingAssociatedReceiver<
          previews::mojom::PreviewsResourceLoadingHintsReceiver> receiver);

  bool IsMainFrame() const;

  mojo::AssociatedReceiver<
      previews::mojom::PreviewsResourceLoadingHintsReceiver>
      receiver_{this};

  blink::mojom::BlinkOptimizationGuideHintsPtr blink_optimization_guide_hints_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoadingHintsAgent);
};

}  // namespace previews

#endif  // CHROME_RENDERER_PREVIEWS_RESOURCE_LOADING_HINTS_AGENT_H_
