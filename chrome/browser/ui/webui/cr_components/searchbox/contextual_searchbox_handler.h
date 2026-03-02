// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class SkBitmap;

namespace contextual_tasks {

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

// Callback type for getting the contextual search session handle.
// Used to allow WebUI controllers to provide session handles to WebUI handlers.
using GetSessionHandleCallback = base::RepeatingCallback<
    contextual_search::ContextualSearchSessionHandle*()>;

class ContextualOmniboxClient : public SearchboxOmniboxClient {
 public:
  ContextualOmniboxClient(Profile* profile, content::WebContents* web_contents);
  ~ContextualOmniboxClient() override;

  using GetSuggestInputsCallback = base::RepeatingCallback<
      std::optional<lens::proto::LensOverlaySuggestInputs>()>;
  void SetSuggestInputsCallback(GetSuggestInputsCallback callback) {
    suggest_inputs_callback_ = std::move(callback);
  }

 protected:
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

 private:
  GetSuggestInputsCallback suggest_inputs_callback_;
};

// This just allows declaration in class to avoid cluttering global namespace.
#define DECLARE_FEATURE(feature) static constinit const base::Feature feature

// Abstract class that extends the SearchboxHandler and implements all methods
// shared between the composebox and realbox to support contextual search.
class ContextualSearchboxHandler
    : public contextual_search::ContextualSearchContextController::
          FileUploadStatusObserver,
      public SearchboxHandler,
      public TabStripModelObserver {
 public:
  DECLARE_FEATURE(kExhaustiveGetRecentTabs);

  explicit ContextualSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxController> controller,
      GetSessionHandleCallback get_session_callback);
  ~ContextualSearchboxHandler() override;

  // searchbox::mojom::PageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     AddTabContextCallback callback) override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void ClearFiles(bool should_block_auto_suggested_tabs) override;
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override;
  void GetInputState(GetInputStateCallback callback) override;

  // Continues the process of adding tab context for a given `tab_id`.
  // This method is used when a `context_token` has already been generated
  // (e.g., by a composebox handler's AddTabContext) and the tab context needs
  // to be associated with that specific token. This differs from
  // `AddTabContext` since `AddTabContext` generates a new context token
  // associated with a session handle.
  void ContinueAddTabContext(int32_t tab_id,
                             bool delay_upload,
                             base::UnguessableToken context_token,
                             AddTabContextCallback callback);

  // Called from browser code (e.g., Views-based file selector) to add file
  // context.
  void AddFileContextFromBrowser(
      std::string file_name,
      std::string mime_type,
      mojo_base::BigBuffer file_bytes,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      AddFileContextCallback callback);

  using RecontextualizeTabCallback = base::OnceCallback<void(bool success)>;
  virtual void UploadTabContextWithData(
      int32_t tab_id,
      std::optional<int64_t> context_id,
      std::unique_ptr<lens::ContextualInputData> data,
      RecontextualizeTabCallback callback);

  // contextual_search::FileUploadStatusObserver:
  void OnFileUploadStatusChanged(
      const base::UnguessableToken& file_token,
      lens::MimeType mime_type,
      contextual_search::FileUploadStatus file_upload_status,
      const std::optional<contextual_search::FileUploadErrorType>& error_type)
      override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  std::optional<lens::ContextualInputData> context_input_data() {
    return context_input_data_;
  }

  // SearchboxHandler:
  omnibox::InputState GetInputState() const override;

  std::vector<base::UnguessableToken> GetUploadedContextTokens();

  contextual_search::InputStateModel* input_state_model() {
    return input_state_model_.get();
  }

  // Resets `input_state_model_`.
  void ResetInputStateModel();
  void SetActiveToolMode(omnibox::ToolMode tool) override;
  void SetActiveModelMode(omnibox::ModelMode model) override;
  void ActivateMetricsFunnel(const std::string& funnel_name) override;

  void OnInputStateChangedForTesting(
      const contextual_search::InputState& state) {
    OnInputStateChanged(state);
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
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTest,
                           ResetInputStateModel);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           SubmitQuery_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           AddTabContext_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           DeleteContext_DelayUpload);

  std::optional<lens::ImageEncodingOptions> CreateTabPreviewEncodingOptions(
      content::WebContents* web_contents);

  contextual_search::ContextualSearchMetricsRecorder* GetMetricsRecorder();

  // Helper function that uploads the cached tab context if it exists.
  void UploadTabContext(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void UploadSnapshotTabContextIfPresent();

  // Returns suggest inputs from the contextual search session, or nullopt if
  // none exists.
  std::optional<lens::proto::LensOverlaySuggestInputs> GetSuggestInputs();

  // Returns the contextual session session handle, or nullptr if none exists.
  // This function also resets the context controller that is being observed for
  // file upload status updates if different from the one that's current.
  contextual_search::ContextualSearchSessionHandle*
  GetContextualSessionHandle();

  // Records metrics for when a tab is added to the composebox.
  void RecordTabAddedMetric(tabs::TabInterface* const tab,
                            bool is_tab_suggestion_chip);

  virtual void InitializeInputStateModel();

  std::unique_ptr<contextual_search::InputStateModel> input_state_model_;

  void OnInputStateChanged(const contextual_search::InputState& state);

  base::CallbackListSubscription input_state_subscription_;

 private:
  // Helper to get the correct number of tab suggestions. Virtual so it
  // can be overridden for specific implementations.
  virtual int GetContextMenuMaxTabSuggestions();

  void OnGetTabPageContext(
      bool delay_upload,
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  // Helper function that handles the caching of the tab context. Once it's
  // successfully cached, we notify the page that the file is uploaded.
  void SnapshotTabContext(
      const base::UnguessableToken& context_token,
      std::unique_ptr<lens::ContextualInputData> page_content_data);

  void OpenUrl(GURL url, const WindowOpenDisposition disposition);

  void OnPreviewReceived(GetTabPreviewCallback callback,
                         const SkBitmap& preview_bitmap);

  std::optional<base::Uuid> GetTaskId();

  std::optional<std::pair<base::UnguessableToken,
                          std::unique_ptr<lens::ContextualInputData>>>
      tab_context_snapshot_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<contextual_tasks::ContextualTasksContextService>
      contextual_tasks_context_service_;
#endif

  // The context controller this searchbox is listening to for file upload
  // status updates.
  base::WeakPtr<contextual_search::ContextualSearchContextController>
      context_controller_;

  std::optional<lens::ContextualInputData> context_input_data_;

  std::unique_ptr<contextual_search::InputState> input_state_;


  // Callback to get the contextual session handle from WebUI controller.
  GetSessionHandleCallback get_session_callback_;

 protected:
  base::WeakPtrFactory<ContextualSearchboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
