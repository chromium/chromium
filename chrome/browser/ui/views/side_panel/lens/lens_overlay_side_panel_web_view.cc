// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_web_view.h"

#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_dismissal_source.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_untrusted_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_LensUntrustedUI =
    SidePanelWebUIViewT<lens::LensUntrustedUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_LensUntrustedUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace {

bool IsEscapeEvent(const content::NativeWebKeyboardEvent& event) {
  return event.GetType() ==
             content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

Browser* BrowserFromWebContents(content::WebContents* web_contents) {
  BrowserWindow* window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
  auto* browser_view = static_cast<BrowserView*>(window);
  if (browser_view) {
    return browser_view->browser();
  }
  return nullptr;
}

}  // namespace

LensOverlaySidePanelWebView::LensOverlaySidePanelWebView(
    Profile* profile,
    lens::LensOverlaySidePanelCoordinator* coordinator)
    : SidePanelWebUIViewT(
          base::RepeatingClosure(),
          base::RepeatingClosure(),
          std::make_unique<WebUIContentsWrapperT<lens::LensUntrustedUI>>(
              GURL(chrome::kChromeUILensUntrustedSidePanelURL),
              profile,
              /*task_manager_string_id=*/IDS_SIDE_PANEL_COMPANION_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false)),
      coordinator_(coordinator) {}

LensOverlaySidePanelWebView::~LensOverlaySidePanelWebView() {
  if (coordinator_) {
    coordinator_->WebViewClosing();
    coordinator_ = nullptr;
  }
}

void LensOverlaySidePanelWebView::ClearCoordinator() {
  coordinator_ = nullptr;
}

bool LensOverlaySidePanelWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

content::WebContents* LensOverlaySidePanelWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  Browser* browser = BrowserFromWebContents(web_contents());
  if (browser) {
    browser->OpenURL(params, std::move(navigation_handle_callback));
  }
  return nullptr;
}

bool LensOverlaySidePanelWebView::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (IsEscapeEvent(event)) {
    Browser* browser = BrowserFromWebContents(web_contents());
    if (browser) {
      content::WebContents* tab_web_contents =
          browser->tab_strip_model()->GetActiveWebContents();
      LensOverlayController* controller =
          LensOverlayController::GetController(tab_web_contents);
      DCHECK(controller);

      if (controller->IsOverlayShowing()) {
        controller->CloseUIAsync(
            lens::LensOverlayDismissalSource::kEscapeKeyPress);
        return true;
      }
    }
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, GetFocusManager());
}

BEGIN_METADATA(LensOverlaySidePanelWebView)
END_METADATA
