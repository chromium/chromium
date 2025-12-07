// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_side_panel_web_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
    SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadAnythingUntrustedUI,
                        SidePanelWebUIViewT);

END_METADATA

ReadAnythingSidePanelWebView::ReadAnythingSidePanelWebView(
    Profile* profile,
    SidePanelEntryScope& scope)
    : SidePanelWebUIViewT(
          scope,
          base::RepeatingClosure(),
          base::RepeatingClosure(),
          std::make_unique<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>(
              GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL),
              profile,
              IDS_READING_MODE_TITLE,
              /*esc_closes_ui=*/false)) {}

ReadAnythingSidePanelWebView::ReadAnythingSidePanelWebView(
    Profile* profile,
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
  if (features::IsImmersiveReadAnythingEnabled()) {
    auto* controller = ReadAnythingController::From(&scope.GetTabInterface());
    if (controller && controller->has_shown_ui()) {
      ShowUI();
    }
  }
}

content::WebContents* ReadAnythingSidePanelWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  ReadAnythingSidePanelController* controller =
      ReadAnythingSidePanelControllerGlue::FromWebContents(web_contents())
          ->controller();
  controller->tab()->GetBrowserWindowInterface()->OpenURL(
      params, std::move(navigation_handle_callback));
  return nullptr;
}

bool ReadAnythingSidePanelWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

base::WeakPtr<ReadAnythingSidePanelWebView>
ReadAnythingSidePanelWebView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
ReadAnythingSidePanelWebView::TakeContentsWrapper() {
  return std::move(contents_wrapper_);
}

ReadAnythingSidePanelWebView::~ReadAnythingSidePanelWebView() = default;

BEGIN_METADATA(ReadAnythingSidePanelWebView)
END_METADATA
