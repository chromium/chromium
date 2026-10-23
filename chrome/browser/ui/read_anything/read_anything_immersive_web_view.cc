// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_immersive_web_view.h"

#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"

ReadAnythingImmersiveWebView::ReadAnythingImmersiveWebView(
    base::OnceClosure on_show_ui_callback,
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        contents_wrapper)
    : on_show_ui_callback_(std::move(on_show_ui_callback)),
      contents_wrapper_(std::move(contents_wrapper)) {
  SetWebContents(contents_wrapper_->web_contents());
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  SetAcceleratorHandling(views::WebContentsAcceleratorHandling::kEnabled);
}

ReadAnythingImmersiveWebView::~ReadAnythingImmersiveWebView() = default;

bool ReadAnythingImmersiveWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

// content::WebContentsDelegate:
// This is called when the WebContents wants to open a link in a new tab,
// such as with the "Search Google for" context menu option. We forward the
// request to the main browser window to handle the navigation.
content::WebContents* ReadAnythingImmersiveWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto* glue = ReadAnythingControllerGlue::FromWebContents(web_contents());
  if (glue && glue->controller()) {
    return glue->controller()->OpenURLFromTab(
        source, params, std::move(navigation_handle_callback));
  }
  return nullptr;
}

bool ReadAnythingImmersiveWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  auto* glue = ReadAnythingControllerGlue::FromWebContents(web_contents());
  if (glue && glue->controller() &&
      glue->controller()->HandleEscapeKey(event)) {
    return true;
  }
  return WebView::HandleKeyboardEvent(source, event);
}

std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingImmersiveWebView::TakeContentsWrapper() {
  // Close tab-modal dialogs (e.g. Print Preview) anchored through this view
  // before it stops hosting them.
  if (features::IsImmersiveReadAnythingPrintEnabled()) {
    auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
        contents_wrapper_->web_contents());
    if (manager) {
      manager->CloseAllDialogs();
    }
  }
  SetWebContents(nullptr);  // This is necessary to reset the web contents.
  contents_wrapper_->SetHost(nullptr);
  SetVisible(false);
  return std::move(contents_wrapper_);
}

// WebUIContentsWrapper::Host:
// Called by the WebUI on its embedder (this class) when the WebUI is ready to
// be shown.
void ReadAnythingImmersiveWebView::ShowUI() {
  // Call SetVisible before running on_show_ui_callback_, in case the callback
  // relies on the visibility of the view.
  SetVisible(true);
  if (on_show_ui_callback_) {
    std::move(on_show_ui_callback_).Run();
  }
}

// Called by the WebUI on its embedder (this class) when the WebUI is ready to
// be closed.
void ReadAnythingImmersiveWebView::CloseUI() {
  // This currently does not do anything and is never called because the
  // ReadAnythingController is currently the one that owns the WebUI by the time
  // it is closed.
}

// Tab-modal dialogs initiated from IRM (e.g. Print Preview) are hosted over
// the tab's contents area in the browser window.
web_modal::WebContentsModalDialogHost*
ReadAnythingImmersiveWebView::GetWebContentsModalDialogHost(
    content::WebContents* web_contents) {
  if (!features::IsImmersiveReadAnythingPrintEnabled()) {
    return nullptr;
  }
  tabs::TabInterface* tab = webui::GetTabInterface(web_contents);
  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  return browser ? browser->GetWebContentsModalDialogHostForTab(tab) : nullptr;
}

BEGIN_METADATA(ReadAnythingImmersiveWebView)
END_METADATA
