// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace lens {
class LensOverlayQueryController;
class LensOverlaySidePanelCoordinator;
class LensPermissionBubbleController;
}  // namespace lens

namespace views {
class View;
class WebView;
}  // namespace views

namespace content {
class WebUI;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace variations {
class VariationsClient;
}  // namespace variations

class PrefService;
class Profile;

// Manages all state associated with the lens overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class LensOverlayController : public LensSearchboxClient,
                              public lens::mojom::LensPageHandler,
                              public lens::mojom::LensSidePanelPageHandler,
                              public content::WebContentsDelegate {
 public:
  LensOverlayController(tabs::TabInterface* tab,
                        variations::VariationsClient* variations_client,
                        signin::IdentityManager* identity_manager,
                        PrefService* pref_service,
                        syncer::SyncService* sync_service);
  ~LensOverlayController() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  // Data struct representing a previous search query.
  struct SearchQuery {
    explicit SearchQuery(std::string text_query, GURL url);
    SearchQuery(const SearchQuery& other);
    SearchQuery& operator=(const SearchQuery& other);
    ~SearchQuery();

    // The text query of the SRP panel.
    std::string search_query_text_;
    // The selected region for this query, if any.
    lens::mojom::CenterRotatedBoxPtr search_query_region_;
    // The selected text for this query, if any.
    std::optional<std::pair<int, int>> selected_text_;
    // The data URI of the thumbnail in the searchbox.
    std::string search_query_region_thumbnail_;
    // The url that the search query loaded into the results frame.
    GURL search_query_url_;
  };

  // Returns whether the lens overlay feature is enabled.
  static bool IsEnabled(Profile* profile);

  // Designates the source of any lens overlay invocation (in other words, any
  // call to `ShowUI()`).
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(InvocationSource)
  enum class InvocationSource {
    // The Chrome app ("3-dot") menu entry.
    kAppMenu = 0,

    // The content area context menu entry that is available when the user
    // right-clicks on any area of the page that doesn't contain text, links or
    // media.
    kContentAreaContextMenuPage = 1,

    // The content area context menu entry that is available when the user
    // right-clicks on an image.
    kContentAreaContextMenuImage = 2,

    // The pinned toolbar action button.
    kToolbar = 3,

    // The find in page (Ctrl/Cmd-f) dialog button.
    kFindInPage = 4,

    // The button in the omnibox (address bar).
    kOmnibox = 5,

    kMaxValue = kOmnibox
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:LensOverlayInvocationSource)

  // This is entry point for showing the overlay UI. This has no effect if state
  // is not kOff. This has no effect if the tab is not in the foreground. If the
  // overlay is successfully invoked, then the value of `invocation_source` will
  // be recorded in the relevant metrics.
  void ShowUI(InvocationSource invocation_source);

  // Designates the source of any lens overlay dismissal (in other words, any
  // call to `CloseUI()`).
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(DismissalSource)
  enum class DismissalSource {
    // The overlay close button (shown when in the kOverlay state).
    kOverlayCloseButton = 0,

    // A click on the background scrim (shown when in the kOverlayAndResults
    // state).
    kOverlayBackgroundClick = 1,

    // The close button in the side panel.
    kSidePanelCloseButton = 2,

    // The pinned toolbar action button.
    kToolbar = 3,

    // The page in the primary web contents changed (link clicked, back button,
    // etc.).
    kPageChanged = 4,

    // The contents of the associated tab were in the background and discarded
    // to save memory.
    kTabContentsDiscarded = 5,

    // The current tab was backgrounded before the screenshot was created.
    kTabBackgroundedWhileScreenshotting = 6,

    // Creating a screenshot from the view of the web contents failed.
    kErrorScreenshotCreationFailed = 7,

    // Encoding the screenshot failed.
    kErrorScreenshotEncodingFailed = 8,

    kMaxValue = kErrorScreenshotEncodingFailed
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/others/enums.xml:LensOverlayDismissalSource)

  // Starts the closing process of the overlay. This is an asynchronous process
  // because we first must unblur the background, before closing the overlay
  // whether. Eventually Calls CloseUI() asynchronously.
  void CloseUIAsync(DismissalSource dismissal_source);

  // Given an instance of `web_ui` created by the LensOverlayController, returns
  // the LensOverlayController. This method is necessary because WebUIController
  // is created by //content with no context or references to the owning
  // controller.
  static LensOverlayController* GetController(content::WebUI* web_ui);

  // Given a `content::WebContents` associated with a tab, returns the
  // associated controller. Returns `nullptr` if there is no controller (e.g.
  // the WebContents is not a tab).
  static LensOverlayController* GetController(
      content::WebContents* tab_contents);

  // Given a `content::WebContents` associated with a glued web view (e.g. side
  // panel), returns the associated controller. Returns `nullptr` if there is no
  // controller glued to the web contents.
  static LensOverlayController* GetControllerFromWebViewWebContents(
      content::WebContents* contents);

  // This method is used to set up communication between this instance and the
  // overlay WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  virtual void BindOverlay(
      mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensPage> page);

  // This method is used to set up communication between this instance and the
  // side panel WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  void BindSidePanel(
      mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensSidePanelPage> page);

  // This method is used to set up communication between this instance and the
  // searchbox WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and has bound the handler. Takes ownership of
  // `handler`.
  void SetSearchboxHandler(std::unique_ptr<RealboxHandler> handler);

  // This method is used to release the owned `SearchboxHandler`. It should be
  // called before the embedding web contents is destroyed since it contains a
  // reference to that web contents.
  void ResetSearchboxHandler();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. There should be no performance overhead as
    // this state will apply to all tabs.
    kOff,

    // In the process of taking a screenshot to transition to kOverlay.
    kScreenshot,

    // In the process of starting the overlay WebUI.
    kStartingWebUI,

    // Showing an overlay without results.
    kOverlay,

    // Showing an overlay with results.
    kOverlayAndResults,

    // The UI has been made inactive / backgrounded and is hidden. This differs
    // from kSuspended as the overlay and web view are not freed and could be
    // immediately reshown.
    kBackground,

    // The UI is currently storing all necessary state for a potential
    // restoration of the overlay view. This frees the overlay and associated
    // views. The following is stored in order to restore the overlay if the
    // user returns:
    // - Screenshot bitmap
    // - The currently selected region, if any
    // - Any overlay objects passed from query controller
    // - Any text passed from query controller
    // - the latest interaction response
    // TODO(b/335516480): Implement suspended state.
    kSuspended,

    // Will be kOff soon.
    kClosing,
  };
  State state() { return state_; }

  // Returns the screenshot currently being displayed on this overlay. If no
  // screenshot is showing, will return nullptr.
  const SkBitmap& current_screenshot() {
    return initialization_data_->current_screenshot_;
  }

  // Returns the side panel coordinator
  lens::LensOverlaySidePanelCoordinator* side_panel_coordinator() {
    return results_side_panel_coordinator_.get();
  }

  // When a tab is in the background, the WebContents may be discarded to save
  // memory. When a tab is in the foreground it is guaranteed to have a
  // WebContents.
  const content::WebContents* tab_contents() { return tab_->GetContents(); }

  // Testing helper method for checking widget.
  views::Widget* GetOverlayWidgetForTesting();

  // Resizes the overlay UI. Used when the window size changes.
  void ResetUIBounds();

  // Creates the glue that allows the WebUIController for a WebView to look up
  // the LensOverlayController.
  void CreateGlueForWebView(views::WebView* web_view);

  // Removes the glue that allows the WebUIController for a WebView to look up
  // the LensOverlayController. Used by the side panel coordinator when it is
  // closed when the overlay is still open. This is a no-op if the provided web
  // view is not glued.
  void RemoveGlueForWebView(views::WebView* web_view);

  // Send text data to the WebUI.
  void SendText(lens::mojom::TextPtr text);

  // Send overlay object data to the WebUI.
  void SendObjects(std::vector<lens::mojom::OverlayObjectPtr> objects);

  // Send message to overlay notifying that the results side panel opened.
  void NotifyResultsPanelOpened();

  // Returns true if the overlay is open and covering the current active tab.
  bool IsOverlayShowing();

  // Pass a result frame URL to load in the side panel.
  void LoadURLInResultsFrame(const GURL& url);

  // Sets the input text for the searchbox. If the searchbox has not been bound,
  // it stores it in `pending_text_query_` instead.
  void SetSearchboxInputText(const std::string& text);

  // Adds a text query to the history stack for this lens overlay. This allows
  // the user to navigate to previous SRP results after sending new queries.
  void AddQueryToHistory(std::string query, GURL search_url);

  // lens::mojom::LensSidePanelPageHandler overrides.
  void PopAndLoadQueryFromHistory() override;

  // Sets whether the results frame should show its loading state.
  virtual void SetSidePanelIsLoadingResults(bool is_loading);

  // Handles when the side panel has been deregistered to do any required
  // cleanup.
  void OnSidePanelEntryDeregistered();

  // Testing function to issue a text request.
  void IssueTextSelectionRequestForTesting(const std::string& text_query,
                                           int selection_start_index,
                                           int selection_end_index);

  // Gets the WebContents housed in the side panel for testing.
  content::WebContents* GetSidePanelWebContentsForTesting();

  // Returns the lens response stored in this controller for testing.
  const lens::proto::LensOverlayInteractionResponse&
  GetLensResponseForTesting() {
    return GetLensResponse();
  }
  // Returns the current page URL for testing.
  const GURL& GetPageURLForTesting() { return GetPageURL(); }

  const std::vector<SearchQuery>& GetSearchQueryHistoryForTesting() {
    return initialization_data_->search_query_history_stack_;
  }

  const std::optional<SearchQuery>& GetLoadedSearchQueryForTesting() {
    return initialization_data_->currently_loaded_search_query_;
  }

  lens::LensPermissionBubbleController*
  GetLensPermissionBubbleControllerForTesting() {
    return permission_bubble_controller_.get();
  }

 protected:
  // Override these methods to stub out network requests for testing.
  virtual std::unique_ptr<lens::LensOverlayQueryController>
  CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_data_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager);

 private:
  // Data class for constructing overlay and storing overlay state for
  // kSuspended state.
  struct OverlayInitializationData {
   public:
    // This is data used to initialize the overlay after the WebUI has been
    // bound to the overlay controller. The only required fields are the
    // screenshot, data URI, and the page information if the data is allowed
    // to be shared. The rest of the fields are optional because the overlay
    // does not require any server response data for use.
    OverlayInitializationData(
        const SkBitmap& screenshot,
        const std::string& data_uri,
        std::optional<GURL> page_url,
        std::optional<std::string> page_title,
        std::vector<lens::mojom::OverlayObjectPtr> objects =
            std::vector<lens::mojom::OverlayObjectPtr>(),
        lens::mojom::TextPtr text = lens::mojom::TextPtr(),
        const lens::proto::LensOverlayInteractionResponse&
            interaction_response = lens::proto::LensOverlayInteractionResponse()
                                       .default_instance(),
        lens::mojom::CenterRotatedBoxPtr selected_region =
            lens::mojom::CenterRotatedBoxPtr());
    ~OverlayInitializationData();

    // Whether there is any full image response data present.
    bool has_full_image_response() const {
      return !text_.is_null() || !objects_.empty();
    }

    // The screenshot that is currently being rendered by the WebUI.
    SkBitmap current_screenshot_;
    std::string current_screenshot_data_uri_;

    // The page url, if it is allowed to be shared.
    std::optional<GURL> page_url_;

    // The page title, if it is allowed to be shared.
    std::optional<std::string> page_title_;

    // The latest stored interaction response from the server.
    lens::proto::LensOverlayInteractionResponse interaction_response_;

    // The selected region. Stored so that it can be used for multiple
    // requests, such as if the user changes the text query without changing
    // the region. Cleared if the user makes a text-only or object selection
    // query.
    lens::mojom::CenterRotatedBoxPtr selected_region_;

    // A pair representing the start and end selection indexes for the currently
    // selected text. This needs to be an optional since std::pair will
    // initialize with default values.
    std::optional<std::pair<int, int>> selected_text_;

    // Text returned from the full image response.
    lens::mojom::TextPtr text_;

    // Overlay objects returned from the full image response.
    std::vector<lens::mojom::OverlayObjectPtr> objects_;

    // The additional query parameters to pass to the query controller for
    // generating urls, set by the search box.
    std::map<std::string, std::string> additional_search_query_params_;

    // A list representing the search query stack that hosts the history of the
    // SRPs the user has navigated to.
    std::vector<SearchQuery> search_query_history_stack_;

    // The search query that is currently loaded in the results frame.
    std::optional<SearchQuery> currently_loaded_search_query_;
  };

  class UnderlyingWebContentsObserver;

  // Called once a screenshot has been captured. This should trigger transition
  // to kOverlay. As this process is asynchronous, there are edge cases that can
  // result in multiple in-flight screenshot attempts. We record the
  // `attempt_id` for each attempt so we can ignore all but the most recent
  // attempt.
  void DidCaptureScreenshot(int attempt_id, const SkBitmap& bitmap);

  // Called when the UI needs to create the overlay widget.
  void ShowOverlayWidget();

  // Backgrounds the UI by hiding the overlay.
  void BackgroundUI();

  // Closes the overlay UI and sets state to kOff. This method is the final
  // cleanup of closing the overlay UI and should only be called by
  // CloseUIAsync. Anyone called trying to close the UI should go through
  // CloseUIAsync.
  void CloseUIPart2(DismissalSource dismissal_source);

  // Passed into the compositor layer to know when the background is done being
  // unblurred and it is safe to close the overlay.
  void OnBackgroundUnblurred(DismissalSource dismissal_source,
                             const viz::FrameTimingDetails& details);

  // Initializes the overlay UI after it has been created with data fetched
  // before its creation.
  void InitializeOverlayUI(const OverlayInitializationData& init_data);

  // Creates InitParams for the overlay widget based on the window bounds.
  views::Widget::InitParams CreateWidgetInitParams();

  // Called when the UI needs to create the view to show in the overlay.
  std::unique_ptr<views::View> CreateViewForOverlay();

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;

  // Overridden from LensSearchboxClient:
  const GURL& GetPageURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override;
  std::string& GetThumbnail() override;
  const lens::proto::LensOverlayInteractionResponse& GetLensResponse()
      const override;
  void OnThumbnailRemoved() override;
  void OnSuggestionAccepted(const GURL& destination_url,
                            AutocompleteMatchType::Type match_type,
                            bool is_zero_prefix_suggestion) override;
  void OnPageBound() override;

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab will enter the background.
  void TabWillEnterBackground(tabs::TabInterface* tab);

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Removes the blur on the live page.
  void RemoveBackgroundBlur();

  // lens::mojom::LensPageHandler overrides.
  void AddBackgroundBlur() override;
  void CloseRequestedByOverlayCloseButton() override;
  void CloseRequestedByOverlayBackgroundClick() override;
  void FeedbackRequestedByOverlay() override;
  // TODO: rename this to IssueRegionSearchRequest.
  void IssueLensRequest(lens::mojom::CenterRotatedBoxPtr region) override;
  void IssueObjectSelectionRequest(const std::string& object_id);
  void IssueTextSelectionRequest(const std::string& text_query,
                                 int selection_start_index,
                                 int selection_end_index) override;

  // Closes search bubble.
  void CloseSearchBubble() override;

  // Handles a request (either region or multimodal) trigger by sending
  // the request to the query controller.
  void IssueSearchBoxRequest(
      const std::string& search_box_text,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion,
      std::map<std::string, std::string> additional_query_params);

  // Handles the response to the Lens start query request.
  void HandleStartQueryResponse(
      std::vector<lens::mojom::OverlayObjectPtr> objects,
      lens::mojom::TextPtr text);

  // Handles the URL response to the Lens interaction request.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Handles the suggest signals response to the Lens interaction request.
  void HandleInteractionDataResponse(
      lens::proto::LensOverlayInteractionResponse response);

  // Handles the creation of a new thumbnail based on the user selection.
  void HandleThumbnailCreated(const std::string& thumbnail_bytes);

  // Sets the thumbnail URI values on the searchbox if it is
  // bound. If it hasn't yet been bound, stores the value in
  // `pending_thumbnail_uri_` instead.
  void SetSearchboxThumbnail(const std::string& thumbnail_uri);

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;

  // A monotonically increasing id. This is used to differentiate between
  // different screenshot attempts.
  int screenshot_attempt_id_ = 0;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // Controller for showing the page screenshot permission bubble.
  std::unique_ptr<lens::LensPermissionBubbleController>
      permission_bubble_controller_;

  // Pointer to the overlay widget.
  views::UniqueWidgetPtr overlay_widget_;
  // Pointer to the web view within the overlay widget if it exists.
  raw_ptr<views::WebView> overlay_web_view_;

  // Pointer to the WebViews that are being glued by this class. Only used to
  // clean up stale pointers. Only valid while `overlay_widget_` is showing.
  std::vector<views::WebView*> glued_webviews_;

  // The assembly data needed for the overlay to be created and shown.
  std::unique_ptr<OverlayInitializationData> initialization_data_;

  // A pending url to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<GURL> pending_side_panel_url_ = std::nullopt;

  // A pending text query to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<std::string> pending_text_query_ = std::nullopt;

  // A pending thumbnail URI to be loaded in the side panel. Needed when the
  // side panel is not bound at the time of a region request.
  std::optional<std::string> pending_thumbnail_uri_ = std::nullopt;

  // Thumbnail URI referencing the data defined by the user image selection on
  // the overlay. If the user hasn't made any selection or has made a text
  // selection this will contain an empty string. Returned by GetThumbnail().
  std::string thumbnail_uri_;

  // Connections to and from the overlay WebUI. Only valid while
  // `overlay_widget_` is showing, and after the WebUI has started executing JS
  // and has bound the connection.
  mojo::Receiver<lens::mojom::LensPageHandler> receiver_{this};
  mojo::Remote<lens::mojom::LensPage> page_;

  // Connections to and from the side panel WebUI. Only valid when the side
  // panel is currently open and after the WebUI has started executing JS and
  // has bound the connection.
  mojo::Receiver<lens::mojom::LensSidePanelPageHandler> side_panel_receiver_{
      this};
  mojo::Remote<lens::mojom::LensSidePanelPage> side_panel_page_;

  // Side panel coordinator for showing results in the panel.
  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
      results_side_panel_coordinator_;

  // Searchbox handler for passing in image and text selections. The handler is
  // null if the WebUI containing the searchbox has not been initialized yet,
  // like in the case of side panel opening. In addition, the handler may be
  // initialized, but the remote not yet set because the WebUI calls SetPage()
  // once it is ready to receive data from C++. Therefore, we must always check
  // that:
  //      1) searchbox_handler_ exists and
  //      2) searchbox_handler_->IsRemoteBound() is true.
  std::unique_ptr<RealboxHandler> searchbox_handler_;

  // Observer for the WebContents of the associated tab. Only valid while the
  // overlay widget is showing.
  std::unique_ptr<UnderlyingWebContentsObserver> tab_contents_observer_;

  // Query controller.
  std::unique_ptr<lens::LensOverlayQueryController>
      lens_overlay_query_controller_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // The pref service associated with the current profile.
  raw_ptr<PrefService> pref_service_;

  // The sync service associated with the current profile.
  raw_ptr<syncer::SyncService> sync_service_;

  // Prevents other features from showing tab-modal UI.
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;

  // Class for handling key events from the renderer that were not handled.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
