// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_search_type_converters.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class SkBitmap;

namespace contextual_tasks {
class ContextualTasksService;

#if !BUILDFLAG(IS_ANDROID)
class ContextualTasksContextService;
#endif
}  // namespace contextual_tasks

namespace lens {
struct ContextualInputData;
struct ImageEncodingOptions;
}  // namespace lens

namespace tabs {
class TabInterface;
}

class ContextualOmniboxClient : public SearchboxOmniboxClient {
 public:
  ContextualOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~ContextualOmniboxClient() override;

 protected:
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

 private:
  contextual_search::ContextualSearchContextController* GetQueryController()
      const;
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
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
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

  std::optional<lens::ContextualInputData> context_input_data() {
    return context_input_data_;
  }

  // For testing.
  std::vector<base::UnguessableToken> GetUploadedContextTokensForTesting() {
    return GetUploadedContextTokens();
  }

 protected:
  void ComputeAndOpenQueryUrl(
      const std::string& query_text,
      WindowOpenDisposition disposition,
      omnibox::ChromeAimEntryPoint aim_entry_point,
      std::map<std::string, std::string> additional_params);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTest,
                           CreateTabPreviewEncodingOptions_NotScaled);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTestDSF2,
                           CreateTabPreviewEncodingOptions_Scaled);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           SubmitQuery_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           AddTabContext_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           DeleteContext_DelayUpload);

  std::optional<lens::ImageEncodingOptions> CreateTabPreviewEncodingOptions(
      content::WebContents* web_contents);

  contextual_search::ContextualSearchMetricsRecorder* GetMetricsRecorder();

  std::vector<base::UnguessableToken> GetUploadedContextTokens();

 protected:
  // Helper function that uploads the cached tab context if it exists.
  void UploadSnapshotTabContextIfPresent();

 private:
  // Helper to get the correct number of tab suggestions. Virtual so it
  // can be overridden for specific implementations.
  virtual int GetContextMenuMaxTabSuggestions();

  void OnAddTabContextTokenCreated(int32_t tab_id,
                                   bool delay_upload,
                                   AddTabContextCallback callback,
                                   const base::UnguessableToken& context_token);

  void OnGetTabPageContext(
      bool delay_upload,
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  // Helper function that handles the caching of the tab context. Once it's
  // successfully cached, we notify the page that the file is uploaded.
  void SnapshotTabContext(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  // Helper Function that does the actual uploading of the tab context.
  void UploadTabContext(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void OpenUrl(GURL url, const WindowOpenDisposition disposition);

  void OnPreviewReceived(GetTabPreviewCallback callback,
                         const SkBitmap& preview_bitmap);

  std::optional<base::Uuid> GetTaskId();
  void AssociateTabWithTask(const base::UnguessableToken& file_token);
  void DisassociateTabsFromTask();

  void RecordTabClickedMetric(tabs::TabInterface* const tab);

  std::optional<std::pair<base::UnguessableToken,
                          std::unique_ptr<lens::ContextualInputData>>>
      tab_context_snapshot_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<contextual_tasks::ContextualTasksContextService>
      contextual_tasks_context_service_;
#endif

  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;

  base::ScopedObservation<contextual_search::ContextualSearchContextController,
                          contextual_search::ContextualSearchContextController::
                              FileUploadStatusObserver>
      file_upload_status_observer_{this};

  std::optional<lens::ContextualInputData> context_input_data_;

  base::WeakPtrFactory<ContextualSearchboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
