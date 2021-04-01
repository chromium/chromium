// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

class ChromeWebContentsHandler
    : public ui::WebDialogWebContentsDelegate::WebContentsHandler {
 public:
  ChromeWebContentsHandler();
  ~ChromeWebContentsHandler() override;

  // Overridden from WebDialogWebContentsDelegate::WebContentsHandler:
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeWebContentsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_WEB_CONTENTS_HANDLER_H_
