// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_web_contents_delegate.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"

WebUIBrowserWebContentsDelegate::WebUIBrowserWebContentsDelegate(
    WebUIBrowserWindow* window)
    : window_(window) {}

WebUIBrowserWebContentsDelegate::~WebUIBrowserWebContentsDelegate() = default;

void WebUIBrowserWebContentsDelegate::SetUIWebContents(
    content::WebContents* ui_web_contents) {
  CHECK(!web_contents());
  Observe(ui_web_contents);
  web_contents()->SetSupportsDraggableRegions(true);
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
  CHECK_EQ(contents, web_contents());
  observers_.Notify(&Observer::DraggableRegionsChanged, regions);
}

content::WebContents* WebUIBrowserWebContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  return window_->browser()->OpenURL(params,
                                     std::move(navigation_handle_callback));
}

void WebUIBrowserWebContentsDelegate::SetFocusToLocationBar() {
  // This is called by WebContentsViewChildFrame implementations in some
  // circumstances (e.g. about:blank), not via user action.
  window_->SetFocusToLocationBar(/*user_initiated=*/false);
}

// TODO(webium): implement ShouldFocusLocationBarByDefault(), perhaps by
// forwarding to the browser.

content::KeyboardEventProcessingResult
WebUIBrowserWebContentsDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return window_->PreHandleKeyboardEvent(event);
}

bool WebUIBrowserWebContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return window_->HandleKeyboardEvent(event);
}
