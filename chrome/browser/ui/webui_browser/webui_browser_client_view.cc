// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_client_view.h"

#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"

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

int WebUIBrowserClientView::NonClientHitTest(const gfx::Point& point) {
  if (draggable_region_.contains(point.x(), point.y())) {
    return HTCAPTION;
  }
  return ClientView::NonClientHitTest(point);
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
