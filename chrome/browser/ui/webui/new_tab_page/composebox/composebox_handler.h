// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/base_composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_metrics_recorder.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "url/gurl.h"

class MetricsReporter;
class Profile;

namespace lens {
struct ContextualInputData;
}

class ComposeboxHandler
    : public composebox::mojom::PageHandler,
      public ComposeboxQueryController::FileUploadStatusObserver,
      public SearchboxHandler,
      public composebox::BaseComposeboxHandler {
 public:
  explicit ComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      std::unique_ptr<ComposeboxQueryController> query_controller,
      std::unique_ptr<ComposeboxMetricsRecorder> metrics_recorder,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter);
  ~ComposeboxHandler() override;

  // This is called from either the ComposeboxOmniboxClient when a match is
  // present in navigation or for the PageHandler's `SubmitQuery()` when there
  // was no match present. The latter only happens when submit is clicked with
  // only a file and no input.
  void SubmitQuery(const std::string& query_text,
                   WindowOpenDisposition disposition) override;

  // composebox::mojom::PageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void FocusChanged(bool focused) override;
  void AddFileContext(composebox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id, AddTabContextCallback) override;
  void DeleteContext(const base::UnguessableToken& file_token) override;
  void ClearFiles() override;
  void GetTabs(GetTabsCallback callback) override;

  // ComposeboxQueryController::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      composebox_query::mojom::FileUploadStatus file_upload_status,
      const std::optional<FileUploadErrorType>& error_type) override;

  // searchbox::mojom::PageHandler:
  void ExecuteAction(uint8_t line,
                     uint8_t action_index,
                     const GURL& url,
                     base::TimeTicks match_selection_timestamp,
                     uint8_t mouse_button,
                     bool alt_key,
                     bool ctrl_key,
                     bool meta_key,
                     bool shift_key) override;
  void PopupElementSizeChanged(const gfx::Size& size) override;
  void OnThumbnailRemoved() override;

 private:
  void OpenUrl(GURL url, const WindowOpenDisposition disposition);

  void OnGetTabPageContext(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  std::unique_ptr<ComposeboxQueryController> query_controller_;
  std::unique_ptr<ComposeboxMetricsRecorder> metrics_recorder_;
  raw_ptr<content::WebContents> web_contents_;

  // These are located at the end of the list of member variables to ensure the
  // WebUI page is disconnected before other members are destroyed.
  mojo::Remote<composebox::mojom::Page> page_;
  mojo::Receiver<composebox::mojom::PageHandler> handler_;

  base::WeakPtrFactory<ComposeboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_COMPOSEBOX_HANDLER_H_
