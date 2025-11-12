// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/searchbox/contextual_search_type_converters.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_omnibox_client.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class SkBitmap;

#if !BUILDFLAG(IS_ANDROID)
namespace contextual_tasks {
class ContextualTasksContextService;
}  // namespace contextual_tasks
#endif

namespace lens {
struct ContextualInputData;
struct ImageEncodingOptions;
}

namespace tabs {
class TabInterface;
}

class ContextualOmniboxClient : public SearchboxOmniboxClient {
 public:
  ContextualOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~ContextualOmniboxClient() override;

 private:
  contextual_search::ContextualSearchContextController* GetQueryController()
      const;
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;
};

// Abstract class that extends the SearchboxHandler and implements all methods
// shared between the composebox and realbox to support contextual search.
class ContextualSearchboxHandler
    : public contextual_search::ContextualSearchContextController::
          FileUploadStatusObserver,
      public SearchboxHandler,
      public TabStripModelObserver {
 public:
  explicit ContextualSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxController> controller);
  ~ContextualSearchboxHandler() override;

  // searchbox::mojom::PageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback) override;
  void DeleteContext(const base::UnguessableToken& file_token) override;
  void ClearFiles() override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override;

  // contextual_search::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

  // SearchboxHandler:
  std::string AutocompleteIconToResourceName(
      const gfx::VectorIcon& icon) const override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 protected:
  void ComputeAndOpenQueryUrl(
      const std::string& query_text,
      WindowOpenDisposition disposition,
      std::map<std::string, std::string> additional_params);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTest,
                           CreateTabPreviewEncodingOptions_NotScaled);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTestDSF2,
                           CreateTabPreviewEncodingOptions_Scaled);

  std::optional<lens::ImageEncodingOptions> CreateTabPreviewEncodingOptions(
      content::WebContents* web_contents);

  contextual_search::ContextualSearchContextController* GetQueryController();

  contextual_search::ContextualSearchMetricsRecorder* GetMetricsRecorder();

  std::set<base::UnguessableToken> deleted_context_tokens() {
    return deleted_context_tokens_;
  }

 private:
  void OnGetTabPageContext(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void OpenUrl(GURL url, const WindowOpenDisposition disposition);

  void OnPreviewReceived(GetTabPreviewCallback callback,
                         const SkBitmap& preview_bitmap);

  void RecordTabClickedMetric(tabs::TabInterface* const tab);

  std::set<base::UnguessableToken> deleted_context_tokens_;
  raw_ptr<content::WebContents> web_contents_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<contextual_tasks::ContextualTasksContextService>
      contextual_tasks_context_service_;
#endif

  base::ScopedObservation<contextual_search::ContextualSearchContextController,
                          contextual_search::ContextualSearchContextController::
                              FileUploadStatusObserver>
      file_upload_status_observer_{this};

  base::WeakPtrFactory<ContextualSearchboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
