// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/draggable_region_host_impl.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

DraggableRegionsHostImpl::DraggableRegionsHostImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<chrome::mojom::DraggableRegions> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

DraggableRegionsHostImpl::~DraggableRegionsHostImpl() = default;

// static
void DraggableRegionsHostImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<chrome::mojom::DraggableRegions> receiver) {
  CHECK(render_frame_host);
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);

  // We only want to bind the receiver for PWAs.
  if (!web_app::AppBrowserController::IsWebApp(browser))
    return;

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new DraggableRegionsHostImpl(*render_frame_host, std::move(receiver));
}

void DraggableRegionsHostImpl::UpdateDraggableRegions(
    std::vector<chrome::mojom::DraggableRegionPtr> draggable_region) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  auto* browser = chrome::FindBrowserWithWebContents(web_contents);
  // When a WebApp browser's WebContents is reparented to a tabbed browser, a
  // draggable regions update may race with the reparenting logic.
  if (!web_app::AppBrowserController::IsWebApp(browser))
    return;

  SkRegion sk_region;
  for (const chrome::mojom::DraggableRegionPtr& region : draggable_region) {
    sk_region.op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.x() + region->bounds.width(),
                          region->bounds.y() + region->bounds.height()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }

  auto* app_browser_controller = browser->app_controller();
  app_browser_controller->UpdateDraggableRegion(sk_region);
}
