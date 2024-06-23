// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_SIDE_PANEL_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_SIDE_PANEL_WEB_VIEW_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "content/public/browser/file_select_listener.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

class Profile;

// CompanionSidePanelWebView holds custom behavior needed for the companion that
// seperates it from other views. This includes display capture access, context
// menu support, and opening urls in a new tab.
class CompanionSidePanelWebView
    : public SidePanelWebUIViewT<CompanionSidePanelUntrustedUI> {
  using SidePanelWebUIViewT_CompanionSidePanelUntrustedUI =
      SidePanelWebUIViewT<CompanionSidePanelUntrustedUI>;
  METADATA_HEADER(CompanionSidePanelWebView,
                  SidePanelWebUIViewT_CompanionSidePanelUntrustedUI)

 public:
  explicit CompanionSidePanelWebView(Profile* profile);
  CompanionSidePanelWebView(const CompanionSidePanelWebView&) = delete;
  CompanionSidePanelWebView& operator=(const CompanionSidePanelWebView&) =
      delete;
  ~CompanionSidePanelWebView() override;

  // SidePanelWebUIViewT:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

 private:
  base::WeakPtrFactory<CompanionSidePanelWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_COMPANION_SIDE_PANEL_WEB_VIEW_H_
