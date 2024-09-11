// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_WEB_VIEW_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_WEB_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "content/public/browser/file_select_listener.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"

namespace lens {
class LensOverlaySidePanelCoordinator;
}  // namespace lens

namespace content {
class BrowserContext;
}  // namespace content

// LensOverlaySidePanelWebView holds custom behavior needed for the lens overlay
// that separates it from other views. This includes context menu support and
// opening urls in a new tab.
class LensOverlaySidePanelWebView
    : public SidePanelWebUIViewT<lens::LensSidePanelUntrustedUI> {
  using SidePanelWebUIViewT_LensSidePanelUntrustedUI =
      SidePanelWebUIViewT<lens::LensSidePanelUntrustedUI>;
  METADATA_HEADER(LensOverlaySidePanelWebView,
                  SidePanelWebUIViewT_LensSidePanelUntrustedUI)

 public:
  LensOverlaySidePanelWebView(
      content::BrowserContext* browser_context,
      lens::LensOverlaySidePanelCoordinator* coordinator);
  LensOverlaySidePanelWebView(const LensOverlaySidePanelWebView&) = delete;
  LensOverlaySidePanelWebView& operator=(const LensOverlaySidePanelWebView&) =
      delete;
  ~LensOverlaySidePanelWebView() override;

  void ClearCoordinator();

  // SidePanelWebUIViewT:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

 private:
  // Indirectly owns this.
  raw_ptr<lens::LensOverlaySidePanelCoordinator> coordinator_;
  base::WeakPtrFactory<LensOverlaySidePanelWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_WEB_VIEW_H_
