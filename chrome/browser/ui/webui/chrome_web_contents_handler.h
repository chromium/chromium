// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_

#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

class ChromeWebContentsHandler
    : public ui::WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  ChromeWebContentsHandler();

  ChromeWebContentsHandler(const ChromeWebContentsHandler&) = delete;
  ChromeWebContentsHandler& operator=(const ChromeWebContentsHandler&) = delete;

  ~ChromeWebContentsHandler() override;

  // Overridden from WebDialogWebContentsDelegate::WebContentsHandler:
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_
