// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_web_view.h"

#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_scope.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
    SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadAnythingUntrustedUI,
                        SidePanelWebUIViewT);

END_METADATA

ReadAnythingSidePanelWebView::ReadAnythingSidePanelWebView(
    SidePanelEntryScope& scope,
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        contents_wrapper)
    : SidePanelWebUIViewT(scope,
                          base::RepeatingClosure(),
                          base::RepeatingClosure(),
                          std::move(contents_wrapper)) {
  // If the UI has been shown once, the reused WebUI will be available but
  // won't send a new "showUI" message. Manually call ShowUI() to unblock the
  // SidePanelEntryWaiter and make the view visible.
  auto* controller = ReadAnythingController::From(&scope.GetTabInterface());
  if (controller && controller->has_shown_ui()) {
    ShowUI();
  }
}

content::WebContents* ReadAnythingSidePanelWebView::OpenURLFromTab(
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

bool ReadAnythingSidePanelWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

bool ReadAnythingSidePanelWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  auto* glue = ReadAnythingControllerGlue::FromWebContents(web_contents());
  if (glue && glue->controller() &&
      glue->controller()->HandleEscapeKey(event)) {
    return true;
  }
  return SidePanelWebUIViewT::HandleKeyboardEvent(source, event);
}

base::WeakPtr<ReadAnythingSidePanelWebView>
ReadAnythingSidePanelWebView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingSidePanelWebView::TakeContentsWrapper() {
  SetWebContents(nullptr);
  contents_wrapper_->SetHost(nullptr);
  return std::move(contents_wrapper_);
}

ReadAnythingSidePanelWebView::~ReadAnythingSidePanelWebView() = default;

BEGIN_METADATA(ReadAnythingSidePanelWebView)
END_METADATA
