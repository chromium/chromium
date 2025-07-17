// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/gurl.h"

class ComposeboxHandler : public composebox::mojom::ComposeboxPageHandler {
 public:
  explicit ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler> handler,
      std::unique_ptr<ComposeboxQueryController> query_controller,
      content::WebContents* web_contents);
  ~ComposeboxHandler() override;

  // composebox::mojom::ComposeboxPageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void AddFile(composebox::mojom::SelectedFileInfoPtr file_info,
               mojo_base::BigBuffer file_bytes) override;

 private:
  void OpenUrl(GURL url, const WindowOpenDisposition disposition);

  mojo::Receiver<composebox::mojom::ComposeboxPageHandler> handler_;
  std::unique_ptr<ComposeboxQueryController> query_controller_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
