// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion_side_panel_web_view.h"

#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/file_select_listener.h"

CompanionSidePanelWebView::CompanionSidePanelWebView(Profile* profile)
    : SidePanelWebUIViewT(
          base::RepeatingClosure(),
          base::RepeatingClosure(),
          std::make_unique<
              BubbleContentsWrapperT<CompanionSidePanelUntrustedUI>>(
              GURL(chrome::kChromeUIUntrustedCompanionSidePanelURL),
              profile,
              /*task_manager_string_id=*/IDS_SIDE_PANEL_COMPANION_TITLE,
              /*webui_resizes_host=*/false,
              /*esc_closes_ui=*/false)) {}

bool CompanionSidePanelWebView::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

content::WebContents* CompanionSidePanelWebView::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  BrowserWindow* window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents());
  auto* browser_view = static_cast<BrowserView*>(window);
  if (browser_view && browser_view->browser()) {
    browser_view->browser()->OpenURL(params);
  }
  return nullptr;
}

void CompanionSidePanelWebView::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  // Note: This is needed for taking screenshots via the feedback form on CSC.
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), /*extension=*/nullptr);
}

void CompanionSidePanelWebView::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

CompanionSidePanelWebView::~CompanionSidePanelWebView() = default;
