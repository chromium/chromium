// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_client_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/base/interaction/element_events.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/interaction/element_tracker_views.h"

WebUIBrowserClientView::WebUIBrowserClientView(
    WebUIBrowserWebContentsDelegate* web_contents_delegate,
    views::Widget* widget,
    views::View* view)
    : ClientView(widget, view), web_contents_delegate_(web_contents_delegate) {
  web_contents_delegate_->AddObserver(this);
}

WebUIBrowserClientView::~WebUIBrowserClientView() {
  web_contents_delegate_->RemoveObserver(this);
  set_contents_view(nullptr);
}

void WebUIBrowserClientView::AddedToWidget() {
  views::ClientView::AddedToWidget();

  // Set up element tracking callback to notify us when the
  // kLocationIconElementId bounds change, so that we can reposition the
  // Permission dialog widget as per the latest position of WebUI Anchor.
  const ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(this);

  CHECK(context);
  // Track location icon element to trigger relayouting when its bounds change.
  location_icon_moved_subscription_ =
      ui::ElementTracker::GetElementTracker()->AddCustomEventCallback(
          ui::kElementBoundsChangedEvent, kLocationIconElementId, context,
          base::BindRepeating(&WebUIBrowserClientView::OnLocationIconMoved,
                              base::Unretained(this)));
}

int WebUIBrowserClientView::NonClientHitTest(const gfx::Point& point) {
  if (draggable_region_.contains(point.x(), point.y())) {
    return HTCAPTION;
  }
  return ClientView::NonClientHitTest(point);
}

void WebUIBrowserClientView::OnLocationIconMoved(ui::TrackedElement* element) {
  BrowserWindowInterface* const browser =
      web_contents_delegate_->window()->browser();
  CHECK(browser);
  content::WebContents* contents =
      browser->GetTabStripModel()->GetActiveWebContents();
  if (contents) {
    auto* manager =
        permissions::PermissionRequestManager::FromWebContents(contents);
    CHECK(manager);
    manager->UpdateAnchor();
  }
}

void WebUIBrowserClientView::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions) {
  SkRegion draggable_region;
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    draggable_region.op(
        SkIRect::MakeXYWH(region->bounds.x(), region->bounds.y(),
                          region->bounds.width(), region->bounds.height()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
  draggable_region_.swap(draggable_region);
}

BEGIN_METADATA(WebUIBrowserClientView)
END_METADATA
