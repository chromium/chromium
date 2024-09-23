// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_WEB_VIEW_H_

#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"

namespace content {
class NavigationHandle;
}

class ReadAnythingSidePanelWebView
    : public SidePanelWebUIViewT<ReadAnythingUntrustedUI> {
  using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
      SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
  METADATA_HEADER(ReadAnythingSidePanelWebView,
                  SidePanelWebUIViewT_ReadAnythingUntrustedUI)
 public:
  explicit ReadAnythingSidePanelWebView(Profile* profile);
  ReadAnythingSidePanelWebView(const ReadAnythingSidePanelWebView&) = delete;
  ReadAnythingSidePanelWebView& operator=(const ReadAnythingSidePanelWebView&) =
      delete;
  ~ReadAnythingSidePanelWebView() override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  base::WeakPtr<ReadAnythingSidePanelWebView> GetWeakPtr();

 private:
  base::WeakPtrFactory<ReadAnythingSidePanelWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_SIDE_PANEL_WEB_VIEW_H_
