// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/bind_post_task.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/lens/contextual_input.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/composebox/composebox_query.mojom.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/omnibox_proto/model_mode.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace content {
class NavigationHandle;
}

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "components/contextual_search/footprints/public/drive_disclaimer_controller.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#endif

class DesktopMediaPickerController;
class DesktopMediaPickerFactory;

namespace content::desktop_capture {
class ScreenshotCaptureRequest;
}

class Profile;
class ContextualSearchboxTabFaviconHelper;
class SkBitmap;
class DrivePickerHostController;
class OmniboxPopupDeactivationBlocker;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class ComposeboxDriveSignInPromoController;
#endif

namespace contextual_tasks {
class ActiveTaskContextProvider;
class ContextualTasksContextService;
class DesktopQueryContextualizerDelegate;
class ContextualTasksUIInterface;
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
  using HasPreviousSubmittedThreadContextCallback =
      base::RepeatingCallback<bool()>;
  using HasAutoSuggestedTabCallback = base::RepeatingCallback<bool()>;
  void SetHasPreviousSubmittedThreadContextCallback(
      HasPreviousSubmittedThreadContextCallback callback) {
    has_previous_submitted_thread_context_callback_ = std::move(callback);
  }
  void SetHasAutoSuggestedTabCallback(HasAutoSuggestedTabCallback callback) {
    has_auto_suggested_tab_callback_ = std::move(callback);
  }

  bool HasPreviousSubmittedThreadContext() const override;
  bool HasAutoSuggestedTab() const override;

  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputsForTesting() const {
    return GetLensOverlaySuggestInputs();
  }

 protected:
  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

 private:
  GetSuggestInputsCallback suggest_inputs_callback_;
  HasPreviousSubmittedThreadContextCallback
      has_previous_submitted_thread_context_callback_;
  HasAutoSuggestedTabCallback has_auto_suggested_tab_callback_;
};

// This just allows declaration in class to avoid cluttering global namespace.
#define DECLARE_FEATURE(feature) static constinit const base::Feature feature

// Abstract class that extends the SearchboxHandler and implements all methods
// shared between the composebox and realbox to support contextual search.
class ContextualSearchboxHandler
    : public contextual_search::ContextualSearchContextController::
          ContextUploadStatusObserver,
      public SearchboxHandler,
      public TabListInterfaceObserver
#if !BUILDFLAG(IS_ANDROID)
    ,
      public drive_picker_host::mojom::DrivePickerResultHandler
#endif
{
 public:
  // TODO(crbug.com/549716561): Refactor screensharing and screenshot capture
  // logic out of ContextualSearchboxHandler into a dedicated controller
  // (similar to DrivePickerHostController).
  class ScreenshareDelegate {
   public:
    virtual ~ScreenshareDelegate() = default;

    virtual void ShowScreenshotMenu(
        const gfx::Rect& anchor_rect,
        base::WeakPtr<ContextualSearchboxHandler> handler) {}

    // Invoked when the screenshare picker is opened or closed.
    virtual void OnScreensharePickerOpened() {}
    virtual void OnScreensharePickerClosed() {}

    using RegionCaptureSource = OmniboxEverywhereService::RegionCaptureSource;
    using RegionSelectedCallback =
        base::OnceCallback<void(const SkBitmap& result_bitmap)>;
    virtual void ShowRegionSelectOverlay(const SkBitmap& screenshot,
                                         const RegionCaptureSource& source,
                                         RegionSelectedCallback callback) {}
  };

  using RegionCaptureSource = ScreenshareDelegate::RegionCaptureSource;

  struct ProcessedScreenshot {
    std::vector<uint8_t> png_bytes;
    std::optional<std::string> thumbnail_data_url;
  };

  using RecontextualizeTabCallback = base::OnceCallback<void(bool)>;

  explicit ContextualSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxClient> client,
      GetSessionHandleCallback get_session_callback,
      ScreenshareDelegate* screenshare_delegate = nullptr);

  ~ContextualSearchboxHandler() override;

  ScreenshareDelegate* screenshare_delegate() const {
    return screenshare_delegate_;
  }
  void set_screenshare_delegate(ScreenshareDelegate* screenshare_delegate) {
    screenshare_delegate_ = screenshare_delegate;
  }

  virtual void SetAimButtonVisible(bool visible) {}

  // searchbox::mojom::PageHandler:
  void NotifySessionStarted() override;
  void NotifySessionAbandoned() override;
  void AddFileContext(searchbox::mojom::SelectedFileInfoPtr file_info,
                      mojo_base::BigBuffer file_bytes,
                      AddFileContextCallback callback) override;
  void AddTabContext(int32_t tab_id,
                     bool delay_upload,
                     searchbox::mojom::TabAttachmentSource source,
                     AddTabContextCallback callback) override;
  void OnDriveUploadClicked(OnDriveUploadClickedCallback callback) override;

  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;
  void DeleteTabContext(int32_t tab_id) override;
  void DeleteContextFromBrowser(const base::UnguessableToken& file_token,
                                bool from_automatic_chip);
  void ClearFiles(bool should_block_auto_suggested_tabs) override;
  void ClearFiles(bool should_block_auto_suggested_tabs, bool query_submitted);
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key,
                   bool is_voice_search) override;
  void GetRecentTabs(GetRecentTabsCallback callback) override;
  void GetTabPreview(int32_t tab_id, GetTabPreviewCallback callback) override;
  void WaitForTabFaviconLoad(int32_t tab_id,
                             WaitForTabFaviconLoadCallback callback) override;
  void GetInputState(GetInputStateCallback callback) override;
  void OpenAutocompleteMatch(uint8_t line,
                             const GURL& url,
                             bool are_matches_showing,
                             uint8_t mouse_button,
                             searchbox::mojom::ActionModifiersPtr modifiers,
                             bool via_keyboard) override;
  void SetSmartComposeStats(
      searchbox::mojom::SmartComposeStatsPtr smart_compose_stats) override;
  void GetDriveDisclaimerStatus(
      GetDriveDisclaimerStatusCallback callback) override;
  void OnDriveDisclaimerAccepted() override;
  void StartScreenshare(bool prefer_entire_screen,
                        StartScreenshareCallback callback) override;
  void CaptureRegionScreenshot(
      CaptureRegionScreenshotCallback callback) override;
  void ShowScreenshotMenu(const gfx::Rect& anchor_rect) override;
#if !BUILDFLAG(IS_ANDROID)
  bool has_drive_picker_deactivation_blocker_for_testing() const {
    return drive_picker_deactivation_blocker_ != nullptr;
  }
  void set_desktop_media_picker_factory_for_testing(
      DesktopMediaPickerFactory* factory) {
    picker_factory_ = factory;
  }
#endif
  void QueryAutocomplete(int32_t query_id,
                         std::optional<int32_t> tab_id,
                         const std::u16string& input,
                         bool prevent_inline_autocomplete,
                         uint32_t cursor_position,
                         omnibox::SuggestInventory suggest_inventory,
                         bool is_on_focus,
                         const std::string& keyword,
                         searchbox::mojom::InputMethod input_method) override;

#if !BUILDFLAG(IS_ANDROID)
  // drive_picker_host::mojom::DrivePickerResultHandler:
  void OnSelection(
      std::vector<drive_picker_host::mojom::DriveFilePtr> files) override;
  void OnCancel() override;
  void OnError(drive_picker_host::mojom::DrivePickerError error) override;
#endif

  // Returns true if smart tab sharing is active for the current query.
  virtual bool IsSmartTabSharingActive() const;

#if !BUILDFLAG(IS_ANDROID)
  void SetSmartTabSharingActive(bool active) override;
  void GetSmartTabSharingActive(
      searchbox::mojom::PageHandler::GetSmartTabSharingActiveCallback callback)
      override;
#endif

  // Returns the list of selected tab IDs that should be transferred.
  virtual std::vector<int32_t> GetSelectedTabIds() const;

  virtual bool SessionHandleHasPreviousSubmittedThreadContext();

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

  using SearchboxHandler::AddFileContextFromBrowser;

  // Called from browser code (e.g., Views-based file selector) to add file
  // context.
  void AddFileContextFromBrowser(
      std::string file_name,
      std::string mime_type,
      mojo_base::BigBuffer file_bytes,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      AddFileContextCallback callback);

  // contextual_search::ContextUploadStatusObserver:
  void OnContextUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type) override;

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override;
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;
  void OnAllTabsAreClosing(TabListInterface& tab_list) override;

  std::optional<lens::ContextualInputData> context_input_data() {
    return context_input_data_;
  }

  std::vector<base::UnguessableToken> GetUploadedContextTokens();

  contextual_search::InputStateModel* input_state_model() {
    return input_state_model_.get();
  }

  // Resets `input_state_model_`.
  void ResetInputStateModel();
  void SetActiveToolMode(omnibox::ToolMode tool, bool is_set_by_aim) override;
  void RecordToolSelectionAction(omnibox::ToolMode tool) override;
  void SetActiveModelMode(omnibox::ModelMode model,
                          bool is_set_by_aim) override;
  void RecordModelSelectionAction(omnibox::ModelMode model) override;
  void ActivateMetricsFunnel(const std::string& funnel_name) override;

  void OnInputStateChangedForTesting(
      const contextual_search::InputState& state) {
    OnInputStateChanged(state);
  }

#if !BUILDFLAG(IS_ANDROID)
  bool ShouldOpenInLensSidePanelForTesting(
      content::WebContents* active_web_contents,
      contextual_search::ContextualSearchSessionHandle* session_handle) {
    return ShouldOpenInLensSidePanel(active_web_contents, session_handle);
  }
#endif

  // Map of context tokens (frontend) to tab IDs (backend);
  // used for determining which tabs to underline based on frontend changes, and
  // for sending `tabID`s to cobrowsing when going from an AIM entrypoint to
  // cobrowsing.
  std::map<base::UnguessableToken, int32_t> selected_tabs;

 protected:
  // SearchboxHandler:
  omnibox::InputState GetInputState() const override;
  // Returns the current input state, re-initializing the underlying model if
  // its weak pointer was invalidated after window startup. Use this instead of
  // `GetInputState() const` when calling from non-const C++ operations.
  omnibox::InputState GetValidInputState();
  std::string GetPreviousQuery() override;

  virtual void ProcessContextAndOpenUrl(
      GURL url,
      const WindowOpenDisposition disposition);

  virtual void OpenUrl(GURL url,
                       const WindowOpenDisposition disposition,
                       base::OnceCallback<void(content::NavigationHandle&)>
                           navigation_handle_callback);

  virtual contextual_tasks::ContextualTasksUIInterface*
  GetContextualTasksUiInterface();

  void ContextualizeQueryAndOpenUrl(
      const std::string& query_text,
      WindowOpenDisposition disposition,
      omnibox::ChromeAimEntryPoint aim_entry_point,
      std::map<std::string, std::string> additional_params,
      bool is_voice_search);

  void ComputeAndOpenQueryUrl(
      const std::string& query_text,
      WindowOpenDisposition disposition,
      omnibox::ChromeAimEntryPoint aim_entry_point,
      std::map<std::string, std::string> additional_params,
      bool is_voice_search);

  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTest,
                           CreateTabPreviewEncodingOptions_NotScaled);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTestDSF2,
                           CreateTabPreviewEncodingOptions_Scaled);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerBrowserTest,
                           ResetInputStateModel);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           SubmitQuery_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           SubmitQuery_TabAttachmentCount);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           AddTabContext_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           AddTabContext_RecentTab);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTestTabsTest,
                           DeleteContext_DelayUpload);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           OpenAutocompleteMatch_ZeroSuggestClick);
  FRIEND_TEST_ALL_PREFIXES(ContextualSearchboxHandlerTest,
                           OpenAutocompleteMatch_TypedSuggestNavigation);

  std::optional<lens::ImageEncodingOptions> CreateTabPreviewEncodingOptions(
      content::WebContents* web_contents);

  // Creates the image encoding options used for uploading images.
  static std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions();

  contextual_search::ContextualSearchMetricsRecorder* GetMetricsRecorder();

  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;

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

#if !BUILDFLAG(IS_ANDROID)
  // Returns true if the query should be opened in the Lens side panel.
  bool ShouldOpenInLensSidePanel(
      content::WebContents* active_web_contents,
      contextual_search::ContextualSearchSessionHandle* session_handle);
#endif

  virtual void InitializeInputStateModel();

  // Returns true if the user/profile is eligible for tab sharing (cobrowse)
  // in contextual search. Defaults to true. Subclasses (such as the side panel
  // composebox) may override this to enforce profile-level eligibility or to
  // return a cached value captured at initialization to prevent jarring UI
  // state changes mid-session.
  virtual bool IsContextualSearchTabSharingEligible() const;

  base::WeakPtr<contextual_search::InputStateModel>
  GetOrCreateInputStateModel();

  void UpdateTabListObservation(TabListInterface* tab_list);

  base::WeakPtr<contextual_search::InputStateModel> input_state_model_;

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

  void OnPreviewReceived(GetTabPreviewCallback callback,
                         const SkBitmap& preview_bitmap);

  void ContextualizeQueryWithRelevantTabsAndOpenUrl(
      const std::string& query_text,
      WindowOpenDisposition disposition,
      omnibox::ChromeAimEntryPoint aim_entry_point,
      std::map<std::string, std::string> additional_params,
      std::vector<base::WeakPtr<content::WebContents>> relevant_tabs);

  std::optional<base::Uuid> GetTaskId() const;

  std::optional<std::pair<base::UnguessableToken,
                          std::unique_ptr<lens::ContextualInputData>>>
      tab_context_snapshot_;

  std::unique_ptr<contextual_tasks::DesktopQueryContextualizerDelegate>
      desktop_delegate_;
  std::unique_ptr<contextual_tasks::QueryContextualizer> query_contextualizer_;

  class ActiveTabNavigationObserver;
  std::unique_ptr<ActiveTabNavigationObserver> active_tab_nav_observer_;

  void OnActiveTabNavigated();

  class AllTabNavigationObserver;
  std::vector<std::unique_ptr<AllTabNavigationObserver>> all_tab_nav_observers_;
  void UpdateAllTabNavigationObservers();
  void OnAnyTabNavigated(content::WebContents* web_contents);

  raw_ptr<contextual_tasks::ContextualTasksContextService>
      contextual_tasks_context_service_;

  // The context controller this searchbox is listening to for file upload
  // status updates.
  base::WeakPtr<contextual_search::ContextualSearchContextController>
      context_controller_;

  std::optional<lens::ContextualInputData> context_input_data_;

  // Callback to get the contextual session handle from WebUI controller.
  GetSessionHandleCallback get_session_callback_;

  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observation_{this};

  std::unique_ptr<ContextualSearchboxTabFaviconHelper> tab_favicon_helper_;

 protected:
  std::optional<bool> smart_tab_sharing_active_for_thread_;
  std::optional<bool> last_sent_smart_tab_sharing_active_;
  bool has_incremented_sts_activation_count_ = false;

  // Gets the `ActiveTaskContextProvider` to update tab underlines.
  contextual_tasks::ActiveTaskContextProvider* GetActiveTaskContextProvider();


  // Cleans up the drive picker controller and result handler receiver.
  // Declared virtual to allow subclasses (such as OmniboxEverywhereHandler) to
  // hook into the cleanup lifetime and coordinate widget focus/dismissal state.
  virtual void CleanupDrivePicker();

#if !BUILDFLAG(IS_ANDROID)
  // Returns true if the user is signed in to the primary account with a valid
  // refresh token that is not in a persistent error state.
  bool IsSignedInWithValidCredentials() const;
  void OnDrivePickerDisconnected();
  void UpdateDriveConsentPref(
      drive_picker::DriveDisclaimerController::DisclaimerStatus status);
  void ShowDrivePicker(
      drive_picker::DriveDisclaimerController::DisclaimerStatus status);
  drive_picker::DriveDisclaimerController* GetDriveDisclaimerController();

  template <typename Method, typename... Args>
  auto BindToUIThread(Method method, Args&&... args) {
    return base::BindPostTask(
        content::GetUIThreadTaskRunner({}),
        base::BindOnce(method, weak_ptr_factory_.GetWeakPtr(),
                       std::forward<Args>(args)...));
  }

  void FallbackToChromeDefaultPicker(bool prefer_entire_screen,
                                     StartScreenshareCallback callback);
  void OnChromeDefaultPickerResults(StartScreenshareCallback callback,
                                    const std::string& err,
                                    content::DesktopMediaID source);
  void OnNativePickerCreated(content::DesktopMediaID::Id id);
  void OnNativePickerSourceSelected(content::DesktopMediaID::Type type,
                                    StartScreenshareCallback callback,
                                    webrtc::DesktopCapturer::Source source);
  void OnNativePickerCancelled(StartScreenshareCallback callback);
  void CaptureAndUploadScreenshot(
      content::DesktopMediaID source,
      StartScreenshareCallback callback,
      std::optional<RegionCaptureSource> region_capture_source = std::nullopt);
  void OnScreenshotCaptured(
      StartScreenshareCallback callback,
      std::optional<RegionCaptureSource> region_capture_source,
      const SkBitmap& bitmap);
  void OnRegionSelected(StartScreenshareCallback callback,
                        const SkBitmap& region_bitmap);
  void OnScreenshotProcessed(StartScreenshareCallback callback,
                             ProcessedScreenshot result);
  void NotifyScreensharePickerOpened();
  void NotifyScreensharePickerClosed();

  mojo::Receiver<drive_picker_host::mojom::DrivePickerResultHandler>
      drive_picker_result_handler_receiver_{this};

  std::unique_ptr<DrivePickerHostController> drive_picker_controller_;

  std::unique_ptr<drive_picker::DriveDisclaimerController>
      drive_disclaimer_controller_;

  // Keeps the AIM popup open while the Google Drive picker is active.
  std::unique_ptr<OmniboxPopupDeactivationBlocker>
      drive_picker_deactivation_blocker_;

  std::unique_ptr<DesktopMediaPickerController> screenshare_picker_controller_;
  raw_ptr<DesktopMediaPickerFactory> picker_factory_ = nullptr;
  std::unique_ptr<content::desktop_capture::ScreenshotCaptureRequest>
      active_screenshot_request_;
  bool is_capturing_ = false;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  std::unique_ptr<ComposeboxDriveSignInPromoController>
      composebox_drive_signin_promo_controller_;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#endif  // !BUILDFLAG(IS_ANDROID)

  // The delegate must outlive this handler, typically implemented by the
  // owning WebUIController.
  raw_ptr<ScreenshareDelegate> screenshare_delegate_ = nullptr;
  OnDriveUploadClickedCallback drive_upload_click_callback_;

  base::WeakPtrFactory<ContextualSearchboxHandler> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
