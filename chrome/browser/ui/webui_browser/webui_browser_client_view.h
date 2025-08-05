// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_CLIENT_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_CLIENT_VIEW_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/webui_browser/webui_browser_web_contents_delegate.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/client_view.h"

class WebUIBrowserClientView
    : public views::ClientView,
      public WebUIBrowserWebContentsDelegate::Observer {
  METADATA_HEADER(WebUIBrowserClientView, views::ClientView)
 public:
  WebUIBrowserClientView(WebUIBrowserWebContentsDelegate* web_contents_delegate,
                         views::Widget* widget,
                         views::View* view);
  ~WebUIBrowserClientView() override;

  // From WebUIBrowserWebContentsDelegate::Observer
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions) override;

  // From ClientView:
  int NonClientHitTest(const gfx::Point& point) override;

 private:
  raw_ptr<WebUIBrowserWebContentsDelegate> web_contents_delegate_;
  SkRegion draggable_region_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSER_WEBUI_BROWSER_CLIENT_VIEW_H_
