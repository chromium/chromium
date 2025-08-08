// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_web_contents_delegate.h"

#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"

WebUIBrowserWebContentsDelegate::WebUIBrowserWebContentsDelegate() = default;

WebUIBrowserWebContentsDelegate::~WebUIBrowserWebContentsDelegate() = default;

void WebUIBrowserWebContentsDelegate::SetUIWebContents(
    content::WebContents* ui_web_contents) {
  CHECK(!ui_web_contents_);
  ui_web_contents_ = ui_web_contents;
  Observe(ui_web_contents_);
}

void WebUIBrowserWebContentsDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebUIBrowserWebContentsDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebUIBrowserWebContentsDelegate::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  // We expect to be used for only the WebUI WebContents.
  CHECK_EQ(contents, ui_web_contents_);
  observers_.Notify(&Observer::DraggableRegionsChanged, regions);
}

void WebUIBrowserWebContentsDelegate::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  EnableDraggableRegions();
}

void WebUIBrowserWebContentsDelegate::EnableDraggableRegions() {
  content::RenderFrameHost* rfh = ui_web_contents_->GetPrimaryMainFrame();
  CHECK(rfh);
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
  client->SetSupportsDraggableRegions(true);
}

void WebUIBrowserWebContentsDelegate::CloseContents(
    content::WebContents* source) {
  // We expect to be used for only the WebUI WebContents.
  CHECK_EQ(source, ui_web_contents_);

  // Don't dangle.
  ui_web_contents_ = nullptr;
}
