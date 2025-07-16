// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/gurl.h"

class ComposeboxHandler
    : public composebox::mojom::PageHandler,
      public ComposeboxQueryController::FileUploadStatusObserver {
 public:
  explicit ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      std::unique_ptr<ComposeboxQueryController> query_controller,
      content::WebContents* web_contents);
  ~ComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void AddFile(composebox::mojom::SelectedFileInfoPtr file_info,
               mojo_base::BigBuffer file_bytes,
               AddFileCallback callback) override;
  void DeleteFile(const base::UnguessableToken& file_token) override;
  void ClearFiles() override;

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      composebox_query::mojom::FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type) override;

 private:
  void OpenUrl(GURL url, const WindowOpenDisposition disposition);

  std::unique_ptr<ComposeboxQueryController> query_controller_;
  raw_ptr<content::WebContents> web_contents_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<composebox::mojom::Page> page_;
  mojo::Receiver<composebox::mojom::PageHandler> handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
