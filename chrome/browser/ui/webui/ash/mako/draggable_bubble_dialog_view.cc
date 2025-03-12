// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/mako/draggable_bubble_dialog_view.h"

#include "chrome/common/chrome_render_frame.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

DraggableBubbleDialogView::DraggableBubbleDialogView(
    WebUIContentsWrapper* contents_wrapper)
    : WebUIBubbleDialogView(nullptr,
                            contents_wrapper->GetWeakPtr(),
                            std::nullopt,
                            views::BubbleBorder::TOP_RIGHT,
                            false) {
  event_handler_ = std::make_unique<MakoBubbleEventHandler>(this);
  dragging_initialized_ = false;
}

DraggableBubbleDialogView::~DraggableBubbleDialogView() = default;

void DraggableBubbleDialogView::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  SkRegion sk_region;
  for (const blink::mojom::DraggableRegionPtr& region : regions) {
    sk_region.op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.x() + region->bounds.width(),
                          region->bounds.y() + region->bounds.height()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }
  draggable_region_ = sk_region;
}

const std::optional<SkRegion> DraggableBubbleDialogView::GetDraggableRegion() {
  return draggable_region_;
}

const gfx::Rect DraggableBubbleDialogView::GetWidgetBoundsInScreen() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return gfx::Rect();
  }
  return widget->GetWindowBoundsInScreen();
}

void DraggableBubbleDialogView::SetWidgetBoundsConstrained(
    const gfx::Rect bounds) {
  if (views::WebView* web_view_ptr = web_view()) {
    web_view_ptr->SetPreferredSize(bounds.size());
  }
  if (views::Widget* widget = GetWidget()) {
    widget->SetBoundsConstrained(bounds);
  }
}

void DraggableBubbleDialogView::SetCursor(const ui::Cursor& cursor) {
  if (views::Widget* widget = GetWidget()) {
    widget->SetCursor(cursor);
  }
}

void DraggableBubbleDialogView::SetupDraggingSupport() {
  // This avoids binding event handler twice.
  if (dragging_initialized_) {
    return;
  }
  dragging_initialized_ = true;
  views::WebView* web_view_ptr = web_view();
  if (!web_view_ptr) {
    return;
  }
  content::WebContents* web_contents = web_view_ptr->GetWebContents();
  if (!web_contents) {
    return;
  }

  // Tell Blink that we support draggable region.
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  if (rfh) {
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetSupportsDraggableRegions(true);
  }

  // Bind event handlers for dragging support.
  GetWidget()->GetNativeView()->AddPreTargetHandler(event_handler_.get());
}

BEGIN_METADATA(DraggableBubbleDialogView)
END_METADATA

}  // namespace ash
