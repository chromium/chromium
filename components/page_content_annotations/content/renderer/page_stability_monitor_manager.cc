// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/renderer/page_stability_monitor_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "components/page_content_annotations/content/renderer/page_stability_monitor.h"
#include "components/page_content_annotations/content/renderer/page_stability_monitor_delegate.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace page_content_annotations {

PageStabilityMonitorManager::PageStabilityMonitorManager(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::PageStabilityMonitorManager>(base::BindRepeating(
          &PageStabilityMonitorManager::OnRenderFrameObserverRequest,
          base::Unretained(this)));
}

PageStabilityMonitorManager::~PageStabilityMonitorManager() = default;

void PageStabilityMonitorManager::CreatePageStabilityMonitor(
    mojo::PendingReceiver<mojom::PageStabilityMonitor> receiver,
    bool supports_paint_stability) {
  monitors_.Add(std::make_unique<PageStabilityMonitor>(
                    *render_frame(), supports_paint_stability,
                    std::make_unique<PageStabilityMonitorDelegate>()),
                std::move(receiver));
}

void PageStabilityMonitorManager::OnDestruct() {
  delete this;
}

void PageStabilityMonitorManager::OnRenderFrameObserverRequest(
    mojo::PendingAssociatedReceiver<mojom::PageStabilityMonitorManager>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace page_content_annotations
