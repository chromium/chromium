// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_MANAGER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_MANAGER_H_

#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace page_content_annotations {

// PageStabilityMonitorManager is the renderer-side implementation of the
// mojom::PageStabilityMonitorManager associated interface.
//
// It serves as a factory for PageStabilityMonitor instances, allowing
// browser-side components (like PageSettledMonitor) to bootstrap
// stability monitoring for a frame without depending on
// chrome/-specific renderer wiring. It manages the lifecycle of
// PageStabilityMonitor instances using a mojo::UniqueReceiverSet.
//
// This ownership model ensures that:
// 1. PageStabilityMonitors are automatically destroyed when the browser-side
// remote is reset or disconnected.
// 2. All PageStabilityMonitors are safely cleaned up if the underlying
// RenderFrame is destroyed, as the PageStabilityMonitorManager is a
// RenderFrameObserver that owns the receiver set.
class PageStabilityMonitorManager : public content::RenderFrameObserver,
                                    public mojom::PageStabilityMonitorManager {
 public:
  explicit PageStabilityMonitorManager(content::RenderFrame* render_frame);
  PageStabilityMonitorManager(const PageStabilityMonitorManager&) = delete;
  PageStabilityMonitorManager& operator=(const PageStabilityMonitorManager&) =
      delete;
  ~PageStabilityMonitorManager() override;

  // mojom::PageStabilityMonitorManager:
  void CreatePageStabilityMonitor(
      mojo::PendingReceiver<mojom::PageStabilityMonitor> receiver,
      bool supports_paint_stability) override;

  // content::RenderFrameObserver:
  void OnDestruct() override;

 private:
  void OnRenderFrameObserverRequest(
      mojo::PendingAssociatedReceiver<mojom::PageStabilityMonitorManager>
          receiver);

  // The set of receivers for the manager interface. Using an associated
  // receiver ensures that monitor creation requests are ordered correctly
  // relative to other frame-level messages and navigations.
  mojo::AssociatedReceiverSet<mojom::PageStabilityMonitorManager> receivers_;

  // The set of active monitors for this frame. Using a UniqueReceiverSet
  // means the manager owns the implementation objects, and their lifetime
  // is automatically tied to the browser-side Mojo remote.
  mojo::UniqueReceiverSet<mojom::PageStabilityMonitor> monitors_;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_RENDERER_PAGE_STABILITY_MONITOR_MANAGER_H_
