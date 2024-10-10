// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/content_extraction/inner_html.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/browser/ui/lens/lens_overlay_blur_layer_delegate.h"
#include "chrome/browser/ui/lens/lens_overlay_colors.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_first_interaction_type.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/sessions/core/session_id.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/views/view_observer.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace lens {
class LensOverlayQueryController;
class LensOverlaySidePanelCoordinator;
class LensPermissionBubbleController;
class LensSearchBubbleController;
class LensOverlayEventHandler;
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

enum class SidePanelEntryHideReason;

class PrefService;
class Profile;

extern void* kLensOverlayPreselectionWidgetIdentifier;

// Manages all state associated with the lens overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class LensOverlayController : public LensSearchboxClient,
                              public lens::mojom::LensPageHandler,
                              public lens::mojom::LensSidePanelPageHandler,
                              public content::WebContentsDelegate,
                              public FullscreenObserver,
                              public SidePanelViewStateObserver,
                              public views::ViewObserver,
                              public views::WidgetObserver,
                              public OmniboxTabHelper::Observer,
                              public content::RenderProcessHostObserver,
                              public find_in_page::FindResultObserver {
 public:
  LensOverlayController(tabs::TabInterface* tab,
                        variations::VariationsClient* variations_client,
                        signin::IdentityManager* identity_manager,
                        PrefService* pref_service,
                        syncer::SyncService* sync_service,
                        ThemeService* theme_service);
  ~LensOverlayController() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  // Data struct representing options for translate data if set.
  struct TranslateOptions {
    std::string source_language;
    std::string target_language;

    TranslateOptions(const std::string& source, const std::string& target)
        : source_language(source), target_language(target) {}
  };

  // Data struct representing a previous search query.
  struct SearchQuery {
    explicit SearchQuery(std::string text_query, GURL url);
    SearchQuery(const SearchQuery& other);
    SearchQuery& operator=(const SearchQuery& other);
    ~SearchQuery();

    // The text query of the SRP panel.
    std::string search_query_text_;
    // The selected region for this query, if any.
    lens::mojom::CenterRotatedBoxPtr selected_region_;
    // The selected region bitmap for this query, if any.
    SkBitmap selected_region_bitmap_;
    // The selected text for this query, if any.
    std::optional<std::pair<int, int>> selected_text_;
    // The data URI of the thumbnail in the searchbox.
    std::string selected_region_thumbnail_uri_;
    // Additional parameters used to build search URLs.
    std::map<std::string, std::string> additional_search_query_params_;
    // The url that the search query loaded into the results frame.
    GURL search_query_url_;
    // The selection type of the current Lens request, if any.
    lens::LensOverlaySelectionType lens_selection_type_;
    // The translate options currently enabled in the overlay.
    std::optional<TranslateOptions> translate_options_;
  };

  // Sets a region to search after the overlay loads, then calls ShowUI().
  // All units are in device pixels. region_bitmap contains the high definition
  // image bytes to use for the search instead of cropping the region from the
  // viewport.
  void ShowUIWithPendingRegion(
      lens::LensOverlayInvocationSource invocation_source,
      const gfx::Rect& tab_bounds,
      const gfx::Rect& view_bounds,
      const gfx::Rect& region_bounds,
      const SkBitmap& region_bitmap);

  // Implementation detail of above, exposed for testing. Do not call this
  // directly.
  void ShowUIWithPendingRegion(
      lens::LensOverlayInvocationSource invocation_source,
      lens::mojom::CenterRotatedBoxPtr region,
      const SkBitmap& region_bitmap);

  // This is entry point for showing the overlay UI. This has no effect if state
  // is not kOff. This has no effect if the tab is not in the foreground. If the
  // overlay is successfully invoked, then the value of `invocation_source` will
  // be recorded in the relevant metrics.
  void ShowUI(lens::LensOverlayInvocationSource invocation_source);

  // Starts the closing process of the overlay. This is an asynchronous process
  // with the following sequence:
  //   (1) Close the side panel
  //   (2) Close the overlay.
  // Step (1) is asynchronous.
  void CloseUIAsync(lens::LensOverlayDismissalSource dismissal_source);

  // Instantly closes the overlay. This may not look nice if the overlay is
  // visible when this is called.
  void CloseUISync(lens::LensOverlayDismissalSource dismissal_source);

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
  void SetSidePanelSearchboxHandler(std::unique_ptr<RealboxHandler> handler);

  // Passes ownership of the realbox handler to the search bubble controller.
  // This is called by the WebUIController when the WebUI is executing
  // javascript and has bound the handler.
  void SetContextualSearchboxHandler(std::unique_ptr<RealboxHandler> handler);

  // This method is used to release the owned `SearchboxHandler`. It should be
  // called before the embedding web contents is destroyed since it contains a
  // reference to that web contents.
  void ResetSidePanelSearchboxHandler();

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. There should be no performance overhead as
    // this state will apply to all tabs.
    kOff,

    // In the process of closing the side panel that was open when the overlay
    // was invoked so we can make a full page screenshot
    kClosingOpenedSidePanel,

    // In the process of taking a screenshot to transition to kOverlay.
    kScreenshot,

    // In the process of starting the overlay WebUI.
    kStartingWebUI,

    // Showing an overlay without results.
    kOverlay,

    // Showing an overlay with results.
    kOverlayAndResults,

    // Showing results with the overlay hidden and live page showing.
    // TODO(b/357121367): Live page with results is no longer related to the
    // overlay and therefore should not exist as a state of the overlay
    // controller. Remove once we have a parent class that can handle this flow.
    kLivePageAndResults,

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

    // In the process of closing the side panel before closing the overlay.
    kClosingSidePanel,

    // Will be kOff soon.
    kClosing,
  };
  State state() { return state_; }

  // Returns the screenshot currently being displayed on this overlay. If no
  // screenshot is showing, will return nullptr.
  const SkBitmap& current_screenshot() {
    return initialization_data_->current_screenshot_;
  }

  // Returns the dynamic color palette identifier based on the screenshot.
  lens::PaletteId color_palette() {
    return initialization_data_->color_palette_;
  }

  // Returns the results side panel coordinator
  lens::LensOverlaySidePanelCoordinator* results_side_panel_coordinator() {
    return results_side_panel_coordinator_.get();
  }

  // When a tab is in the background, the WebContents may be discarded to save
  // memory. When a tab is in the foreground it is guaranteed to have a
  // WebContents.
  const content::WebContents* tab_contents() { return tab_->GetContents(); }

  // Returns the event handler for this instance of the Lens Overlay.
  lens::LensOverlayEventHandler* lens_overlay_event_handler() {
    return lens_overlay_event_handler_.get();
  }

  // Returns invocation time since epoch. Used to set up html source for metric
  // logging.
  uint64_t GetInvocationTimeSinceEpoch();

  // Testing helper method for checking view housing our overlay.
  views::View* GetOverlayViewForTesting();

  // Testing helper method for checking web view.
  views::WebView* GetOverlayWebViewForTesting();

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

  // Creates theme with data obtained from `palette_id` to be sent to the WebUI.
  lens::mojom::OverlayThemePtr CreateTheme(lens::PaletteId palette_id);

  // Send overlay object data to the WebUI.
  void SendObjects(std::vector<lens::mojom::OverlayObjectPtr> objects);

  // Send message to overlay notifying that the results side panel opened.
  void NotifyResultsPanelOpened();

  // Send message to overlay to copy the currently selected text.
  void TriggerCopyText();

  // Returns true if the overlay is open and covering the current active tab.
  bool IsOverlayShowing();

  // Returns true if the overlay is currently in the process of closing.
  bool IsOverlayClosing();

  // Pass a result frame URL to load in the side panel.
  void LoadURLInResultsFrame(const GURL& url);

  // Adds a text query to the history stack for this lens overlay. This allows
  // the user to navigate to previous SRP results after sending new queries.
  void AddQueryToHistory(std::string query, GURL search_url);

  // lens::mojom::LensSidePanelPageHandler overrides.
  void PopAndLoadQueryFromHistory() override;

  // Sets whether the results frame should show its loading state.
  virtual void SetSidePanelIsLoadingResults(bool is_loading);
  // Sets whether the side panel should show a full error page.
  virtual void SetSidePanelShowErrorPage(bool should_show_error_page);

  // Called before the lens results panel begins hiding. This is called before
  // any side panel closing animations begin.
  void OnSidePanelWillHide(SidePanelEntryHideReason reason);

  // Called when the lens side panel has been hidden.
  void OnSidePanelHidden();

  tabs::TabInterface* GetTabInterface();

  // Show preselection toast bubble. Creates a preselection bubble if it does
  // not exist.
  void ShowPreselectionBubble();

  // Hides preselection toast bubble. Used when backgrounding the overlay. This
  // hides the widget associated with the bubble.
  void HidePreselectionBubble();

  // Shows the feedback page.
  void FeedbackRequestedByEvent(int event_flags);

  // Shows the info page.
  void InfoRequestedByEvent(int event_flags);

  // Shows My Activity.
  void ActivityRequestedByEvent(int event_flags);

  // Testing function to issue a Lens region selection request.
  void IssueLensRegionRequestForTesting(lens::mojom::CenterRotatedBoxPtr region,
                                        bool is_click);

  // Testing function to issue a text request.
  void IssueTextSelectionRequestForTesting(const std::string& text_query,
                                           int selection_start_index,
                                           int selection_end_index);

  // Testing function to issue a text request.
  void RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction user_action);

  // Testing function to issue a translate request.
  void IssueTranslateSelectionRequestForTesting(
      const std::string& text_query,
      const std::string& content_language,
      int selection_start_index,
      int selection_end_index);

  // Testing function to issue a full page translate request.
  void IssueTranslateFullPageRequestForTesting(
      const std::string& source_language,
      const std::string& target_language);

  // Testing function to end translate mode.
  void IssueEndTranslateModeRequestForTesting();

  // Testing function to issue a searchbox request.
  void IssueSearchBoxRequestForTesting(
      const std::string& search_box_text,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion,
      std::map<std::string, std::string> additional_query_params);

  // Gets string for invocation source enum, used for logging metrics.
  std::string GetInvocationSourceString();

  // Gets the WebContents housed in the side panel for testing.
  content::WebContents* GetSidePanelWebContentsForTesting();

  // Returns the current page URL for testing.
  const GURL& GetPageURLForTesting();

  // Returns the current tab ID for testing.
  SessionID GetTabIdForTesting();

  // Returns the current searchbox page classification for testing.
  metrics::OmniboxEventProto::PageClassification
  GetPageClassificationForTesting();

  // Returns the current thumbnail URI for testing.
  const std::string& GetThumbnailForTesting();

  // Handles the event where text was modified in the searchbox for testing.
  void OnTextModifiedForTesting();

  // Handles the event where the thumbnail was removed from the searchbox for
  // testing.
  void OnThumbnailRemovedForTesting();

  // Returns the lens suggest inputs stored in this controller for testing.
  const lens::proto::LensOverlaySuggestInputs& GetLensSuggestInputsForTesting();

  const lens::mojom::CenterRotatedBoxPtr& get_selected_region_for_testing() {
    return initialization_data_->selected_region_;
  }

  const std::optional<std::pair<int, int>> get_selected_text_for_region() {
    return initialization_data_->selected_text_;
  }

  const std::vector<SearchQuery>& get_search_query_history_for_testing() {
    return initialization_data_->search_query_history_stack_;
  }

  const std::optional<SearchQuery>& get_loaded_search_query_for_testing() {
    return initialization_data_->currently_loaded_search_query_;
  }

  const std::vector<lens::mojom::CenterRotatedBoxPtr>&
  GetSignificantRegionBoxesForTesting() {
    return initialization_data_->significant_region_boxes_;
  }

  lens::LensPermissionBubbleController*
  get_lens_permission_bubble_controller_for_testing() {
    return permission_bubble_controller_.get();
  }

  views::Widget* get_preselection_widget_for_testing() {
    return preselection_widget_.get();
  }

  lens::LensSearchBubbleController*
  get_lens_search_bubble_controller_for_testing() {
    return search_bubble_controller_.get();
  }

  lens::LensOverlayQueryController*
  get_lens_overlay_query_controller_for_testing() {
    return lens_overlay_query_controller_.get();
  }

 protected:
  // Override these methods to stub out network requests for testing.
  virtual std::unique_ptr<lens::LensOverlayQueryController>
  CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller);

 private:
  // Data class for constructing overlay and storing overlay state for
  // kSuspended state.
  struct OverlayInitializationData {
   public:
    // This is data used to initialize the overlay after the WebUI has been
    // bound to the overlay controller. The only required fields are the
    // screenshot, data URI, and the page information if the data is allowed
    // to be shared. The rest of the fields are optional because the overlay
    // does not require any server response data for use. rgb_screenshot passes
    // ownership of the Bitmap to OverlayInitializationData.
    OverlayInitializationData(const SkBitmap& screenshot,
                              SkBitmap rgb_screenshot,
                              lens::PaletteId color_palette,
                              std::optional<GURL> page_url,
                              std::optional<std::string> page_title);
    ~OverlayInitializationData();

    // Whether there is any full image response data present.
    bool has_full_image_response() const {
      return !text_.is_null() || !objects_.empty();
    }

    // The screenshot that is currently being rendered by the WebUI.
    // current_screenshot_ is in native format and is needed to encode JPEGs to
    // send to the server. current_rgb_screenshot_ is in RGBA color type and
    // used to display in the WebUI. current_rgb_screenshot_ cannot be used to
    // encode JPEGs because the JPEG encoder expects the native color format.
    SkBitmap current_screenshot_;
    SkBitmap current_rgb_screenshot_;

    // The dynamic color palette identifier based on the screenshot.
    lens::PaletteId color_palette_;

    // The page url, if it is allowed to be shared.
    std::optional<GURL> page_url_;

    // The page title, if it is allowed to be shared.
    std::optional<std::string> page_title_;

    // The bytes of the content the user is viewing, if the bytes are able to be
    // retrieved.
    std::vector<uint8_t> page_content_bytes_;

    // The mime type of page_content_bytes_. kNone if page_content_bytes_is
    // empty.
    lens::PageContentMimeType page_content_type_ =
        lens::PageContentMimeType::kNone;

    // Bounding boxes for significant regions identified in the screenshot.
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes_;

    // The latest suggest inputs from the query controller.
    lens::proto::LensOverlaySuggestInputs suggest_inputs_;

    // The selected region. Stored so that it can be used for multiple
    // requests, such as if the user changes the text query without changing
    // the region. Cleared if the user makes a text-only or object selection
    // query.
    lens::mojom::CenterRotatedBoxPtr selected_region_;

    // The selected region bitmap. This should only be set if the user opened
    // the overlay with a pending region bitmap. Stored so that it can be used
    // for multiple requests, such as if the user changes the text query without
    // changing the region. Cleared if the user makes a text-only or object
    // selection query.
    SkBitmap selected_region_bitmap_;

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

    // The translate options currently enabled in the overlay.
    std::optional<TranslateOptions> translate_options_;
  };

  class UnderlyingWebContentsObserver;

  // Takes a screenshot of the current viewport.
  void CaptureScreenshot();

  // Fetches the bounding boxes of all images within the current viewport.
  void FetchViewportImageBoundingBoxes(const SkBitmap& bitmap);

  // Called once a screenshot has been captured. This should trigger transition
  // to kOverlay. As this process is asynchronous, there are edge cases that can
  // result in multiple in-flight screenshot attempts. We record the
  // `attempt_id` for each attempt so we can ignore all but the most recent
  // attempt.
  // `chrome_render_frame` is added to keep the InterfacePtr alive during the
  // IPC call in FetchViewportImageBoundingBoxes().
  void DidCaptureScreenshot(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      int attempt_id,
      const SkBitmap& bitmap,
      const std::vector<gfx::Rect>& bounds);

  // Process the bitmap and creates all necessary data to initialize the
  // overlay. Happens on a separate thread to prevent main thread from hanging.
  void CreateInitializationData(const SkBitmap& screenshot,
                                const std::vector<gfx::Rect>& all_bounds);

  // Called after creating the RGB bitmap and we are back on the main thread.
  void ContinueCreateInitializationData(
      const SkBitmap& screenshot,
      const std::vector<gfx::Rect>& all_bounds,
      SkBitmap rgb_screenshot);

#if BUILDFLAG(ENABLE_PDF)
  // Receives the PDF bytes from the IPC call to the PDF renderer and stores
  // them in initialization data.
  void OnPdfBytesReceived(
      std::unique_ptr<OverlayInitializationData> initialization_data,
      pdf::mojom::PdfListener::GetPdfBytesStatus status,
      const std::vector<uint8_t>& bytes);
#endif  // BUILDFLAG(ENABLE_PDF)

  // Callback for when the inner text is retrieved from the underlying page.
  void OnInnerTextReceived(
      std::unique_ptr<OverlayInitializationData> initialization_data,
      std::unique_ptr<content_extraction::InnerTextResult> result);

  // Callback for when the inner HTML is retrieved from the underlying page.
  void OnInnerHtmlReceived(
      std::unique_ptr<OverlayInitializationData> initialization_data,
      const std::optional<std::string>& result);

  // Adds bounding boxes to the initialization data.
  void AddBoundingBoxesToInitializationData(
      OverlayInitializationData* initialization_data,
      const std::vector<gfx::Rect>& bounds);

  // Enables/disables the background blur updating live. This should be used to
  // save resources on blurring the background when not needed.
  void SetLiveBlur(bool enabled);

  // Called when the UI needs to show the overlay via a view that is a child of
  // the tab contents view.
  void ShowOverlay();

  // Backgrounds the UI by hiding the overlay.
  void BackgroundUI();

  // Closes the overlay UI and sets state to kOff. This method is the final
  // cleanup of closing the overlay UI. This resets all state internal to the
  // LensOverlayController.
  // Anyone called trying to close the UI should go through CloseUIAsync or
  // CloseUISync. Those methods also reset state external to
  // LensOverlayController.
  void CloseUIPart2(lens::LensOverlayDismissalSource dismissal_source);

  // Initializes all parts of our UI and starts the query flow.
  // Runs once the overlay WebUI and initialization data are both ready.
  // Once initialization_data is ready, it should be passed to this method to be
  // cached until all parts of the flow are ready. Parts of the initialization
  // flow (like creating WebUI) that do not touch initialization_data should
  // pass initialization_data as nullptr.
  void InitializeOverlay(
      std::unique_ptr<OverlayInitializationData> initialization_data);

  // Initializes the overlay UI after it has been created with data fetched
  // before its creation.
  void InitializeOverlayUI(const OverlayInitializationData& init_data);

  // Called when the UI needs to create the view to show in the overlay.
  std::unique_ptr<views::View> CreateViewForOverlay();

  // Clears the selected region.
  void ClearRegionSelection();

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  // ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // OmniboxTabHelper::Observer:
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

  // find_in_page::FindResultObserver:
  void OnFindEmptyText(content::WebContents* web_contents) override;
  void OnFindResultAvailable(content::WebContents* web_contents) override;

  // Overridden from LensSearchboxClient:
  const GURL& GetPageURL() const override;
  SessionID GetTabId() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override;
  std::string& GetThumbnail() override;
  const lens::proto::LensOverlaySuggestInputs& GetLensSuggestInputs()
      const override;
  void OnTextModified() override;
  void OnThumbnailRemoved() override;
  void OnSuggestionAccepted(const GURL& destination_url,
                            AutocompleteMatchType::Type match_type,
                            bool is_zero_prefix_suggestion) override;
  void OnPageBound() override;

  // SidePanelViewStateObserver:
  void OnSidePanelDidOpen() override;
  void OnSidePanelCloseInterrupted() override;
  void OnSidePanelDidClose() override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab will enter the background.
  void TabWillEnterBackground(tabs::TabInterface* tab);

  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  // Called when the tab will be removed from the window.
  void WillDetach(tabs::TabInterface* tab,
                  tabs::TabInterface::DetachReason reason);

  // Makes a Lens request and updates all state related to the Lens request. If
  // region_bitmap is provided, it will use those bytes to send to the Lens
  // server instead of cropping the region from the full page screenshot.
  void DoLensRequest(lens::mojom::CenterRotatedBoxPtr region,
                     lens::LensOverlaySelectionType selection_type,
                     std::optional<SkBitmap> region_bitmap);

  // Suggest a name for the save as image feature incorporating the hostname of
  // the page. Protocol, TLD, etc are not taken into consideration. Duplicate
  // names get automatic suffixes.
  static const std::u16string GetFilenameForURL(const GURL& url);

  // lens::mojom::LensPageHandler overrides.
  void ActivityRequestedByOverlay(
      ui::mojom::ClickModifiersPtr click_modifiers) override;
  void AddBackgroundBlur() override;
  void ClosePreselectionBubble() override;
  void CloseRequestedByOverlayCloseButton() override;
  void CloseRequestedByOverlayBackgroundClick() override;
  void CloseSearchBubble() override;
  void CopyImage(lens::mojom::CenterRotatedBoxPtr region) override;
  void CopyText(const std::string& text) override;
  void FeedbackRequestedByOverlay() override;
  void GetOverlayInvocationSource(
      GetOverlayInvocationSourceCallback callback) override;
  void InfoRequestedByOverlay(
      ui::mojom::ClickModifiersPtr click_modifiers) override;
  void IssueLensObjectRequest(lens::mojom::CenterRotatedBoxPtr region,
                              bool is_mask_click) override;
  void IssueLensRegionRequest(lens::mojom::CenterRotatedBoxPtr region,
                              bool is_click) override;
  void IssueTextSelectionRequest(const std::string& text_query,
                                 int selection_start_index,
                                 int selection_end_index) override;
  void IssueTranslateFullPageRequest(
      const std::string& source_language,
      const std::string& target_language) override;
  void IssueEndTranslateModeRequest() override;
  void IssueTranslateSelectionRequest(const std::string& text_query,
                                      const std::string& content_language,
                                      int selection_start_index,
                                      int selection_end_index) override;
  void NotifyOverlayInitialized() override;
  void RecordUkmAndTaskCompletionForLensOverlayInteraction(
      lens::mojom::UserAction user_action) override;
  void RecordLensOverlaySemanticEvent(
      lens::mojom::SemanticEvent event) override;
  void SaveAsImage(lens::mojom::CenterRotatedBoxPtr region) override;
  void MaybeShowTranslateFeaturePromo() override;
  void MaybeCloseTranslateFeaturePromo() override;

  // Tries to show the translate feature promo after the translate button
  // element is shown.
  void TryShowTranslateFeaturePromo(ui::TrackedElement* element);

  // Performs shared logic for IssueTextSelectionRequest() and
  // IssueTranslateSelectionRequest().
  void IssueTextSelectionRequestInner(const std::string& text_query,
                                      int selection_start_index,
                                      int selection_end_index);

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
      lens::mojom::TextPtr text,
      bool is_error);

  // Handles the URL response to the Lens interaction request.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Handles an update to the suggest inputs. This will be called whenever
  // any part of the suggest inputs changes, such as when a new objects
  // request is sent, or when an interaction data response is received.
  void HandleSuggestInputsResponse(
      lens::proto::LensOverlaySuggestInputs suggest_inputs);

  // Handles the creation of a new thumbnail based on the user selection.
  void HandleThumbnailCreated(const std::string& thumbnail_bytes);

  // Sets the input text for the searchbox. If the searchbox has not been bound,
  // it stores it in `pending_text_query_` instead.
  void SetSearchboxInputText(const std::string& text);

  // Sets the thumbnail URI values on the searchbox if it is
  // bound. If it hasn't yet been bound, stores the value in
  // `pending_thumbnail_uri_` instead.
  void SetSearchboxThumbnail(const std::string& thumbnail_uri);

  // Records UMA and UKM metrics for time to first interaction. Not recorded
  // when invocation source is an image's content area menu because in this
  // case the time to first interaction is essentially zero.
  void RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType interaction_type);

  // Records UMA and UKM metrics for dismissal and end of session metrics.
  // This includes dismissal source, session length, and whether a search was
  // recorded in the session.
  void RecordEndOfSessionMetrics(
      lens::LensOverlayDismissalSource dismissal_source);

  // Launches the Lens overlay HaTS survey if eligible.
  void MaybeLaunchSurvey();

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

  // Pointer to the WebViews that are being glued by this class. Only used to
  // clean up stale pointers. Only valid while `overlay_view_` is showing.
  std::vector<views::WebView*> glued_webviews_;

  // The assembly data needed for the overlay to be created and shown.
  std::unique_ptr<OverlayInitializationData> initialization_data_;

  // Invocation source for the lens overlay.
  lens::LensOverlayInvocationSource invocation_source_;

  // A pending url to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<GURL> pending_side_panel_url_ = std::nullopt;

  // A pending text query to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<std::string> pending_text_query_ = std::nullopt;

  // A pending thumbnail URI to be loaded in the side panel. Needed when the
  // side panel is not bound at the time of a region request.
  std::optional<std::string> pending_thumbnail_uri_ = std::nullopt;

  // Whether the pending side panel should open with the error page showing.
  // This happens when the full image request resulted in an error response.
  bool pending_side_panel_should_show_error_page_ = false;

  // Pending region to search after the overlay loads.
  lens::mojom::CenterRotatedBoxPtr pending_region_;

  // The bitmap for the pending region stored in pending_region_.
  // pending_region_ and pending_region_bitmap_ are correlated and their
  // lifecycles are should stay in sync.
  SkBitmap pending_region_bitmap_;

  // If the side panel needed to be closed before dismissing the overlay, this
  // stores the original dismissal_source so it is properly recorded when the
  // side panel is done closing and the callback is invoked.
  std::optional<lens::LensOverlayDismissalSource> last_dismissal_source_;

  // Thumbnail URI referencing the data defined by the user image selection on
  // the overlay. If the user hasn't made any selection or has made a text
  // selection this will contain an empty string. Returned by GetThumbnail().
  std::string selected_region_thumbnail_uri_;

  // The selection type of the current Lens request. If the
  // user is not currently viewing results for a Lens query, this will be
  // set to UNKNOWN_SELECTION_TYPE.
  lens::LensOverlaySelectionType lens_selection_type_ =
      lens::UNKNOWN_SELECTION_TYPE;

  // Connections to and from the overlay WebUI. Only valid while
  // `overlay_view_` is showing, and after the WebUI has started executing JS
  // and has bound the connection.
  mojo::Receiver<lens::mojom::LensPageHandler> receiver_{this};
  mojo::Remote<lens::mojom::LensPage> page_;

  // Connections to and from the side panel WebUI. Only valid when the side
  // panel is currently open and after the WebUI has started executing JS and
  // has bound the connection.
  mojo::Receiver<lens::mojom::LensSidePanelPageHandler> side_panel_receiver_{
      this};
  mojo::Remote<lens::mojom::LensSidePanelPage> side_panel_page_;

  // Observer for the WebContents of the associated tab. Only valid while the
  // overlay view is showing.
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

  // The theme service associated with the current profile.
  raw_ptr<ThemeService> theme_service_;

  // Prevents other features from showing tab-modal UI.
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;

  // Indicates whether a search has been performed in the current session. Used
  // to record success/abandonment rate, as defined by whether or not a search
  // was performed.
  bool search_performed_in_session_ = false;

  // The time at which the overlay was invoked. Used to compute timing metrics.
  base::TimeTicks invocation_time_;

  // The time at which the overlay was invoked, since epoch. Used to calculate
  // timeToWebUIReady on the WebUI side.
  base::Time invocation_time_since_epoch_;

  // Indicates whether a trigger for the HaTS survey has occurred in the current
  // session. Note that a trigger does not mean the survey will actually be
  // shown.
  bool hats_triggered_in_session_ = false;

  // The callback subscription for the element shown callback used to show the
  // translate feature promo.
  base::CallbackListSubscription translate_button_shown_subscription_;

  // ---------------Browser window scoped state: START---------------------
  // State that is scoped to the browser window must be reset when the tab is
  // backgrounded, since the tab may move between browser windows.

  // Observes the side panel of the browser window.
  base::ScopedObservation<SidePanelCoordinator, SidePanelViewStateObserver>
      side_panel_state_observer_{this};

  // Observer to check for browser window entering fullscreen.
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observation_{this};

  // Observer to check if the user is using CTRL/CMD+F while the overlay is
  // open.
  base::ScopedObservation<find_in_page::FindTabHelper,
                          find_in_page::FindResultObserver>
      find_tab_observer_{this};

  // Observer to check when the content web view bounds change.
  base::ScopedObservation<views::View, views::ViewObserver>
      tab_contents_view_observer_{this};

  // Observer to check when the preselection widget is deleted.
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      preselection_widget_observer_{this};

  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_tab_helper_observer_{this};

  // The controller for sending gen204 pings. Owned by the overlay controller
  // so that the life cycle outlasts the query controller, allowing gen204
  // requests to be sent upon query end.
  std::unique_ptr<lens::LensOverlayGen204Controller> gen204_controller_;

  // Owns the search bubble that shows over the overlay, before the side panel
  // is showing.
  std::unique_ptr<lens::LensSearchBubbleController> search_bubble_controller_;

  // Searchbox handler for passing in image and text selections. The handler is
  // null if the WebUI containing the searchbox has not been initialized yet,
  // like in the case of side panel opening. In addition, the handler may be
  // initialized, but the remote not yet set because the WebUI calls SetPage()
  // once it is ready to receive data from C++. Therefore, we must always check
  // that:
  //      1) searchbox_handler_ exists and
  //      2) searchbox_handler_->IsRemoteBound() is true.
  std::unique_ptr<RealboxHandler> side_panel_searchbox_handler_;

  // Handler for the contextual searchbox in the overlay . The handler is
  // null if the WebUI containing the searchbox has not been initialized yet.
  // In addition, the handler may be initialized, but the remote not yet set
  // because the WebUI calls SetPage() once it is ready to receive data from
  // C++. Therefore, we must always check that:
  //      1) contextual_searchbox_handler_ exists and
  //      2) contextual_searchbox_handler_->IsRemoteBound() is true.
  std::unique_ptr<RealboxHandler> overlay_searchbox_handler_;

  // General side panel coordinator responsible for all side panel interactions.
  // Separate from the results_side_panel_coordinator because this controls
  // interactions to other side panels as well, not just our results. The
  // side_panel_coordinator lives with the browser view, so it should outlive
  // this class. Therefore, if the controller is not in the kOff state, this can
  // be assumed to be non-null.
  raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;

  // Side panel coordinator for showing results in the panel.
  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
      results_side_panel_coordinator_;

  // Class for handling key events from the renderer that were not handled.
  std::unique_ptr<lens::LensOverlayEventHandler> lens_overlay_event_handler_;

  // Layer delegate that handles blurring the background behind the WebUI.
  std::unique_ptr<lens::LensOverlayBlurLayerDelegate>
      lens_overlay_blur_layer_delegate_;

  // Pointer to the view that houses our overlay as a child of the tab
  // contents web view.
  raw_ptr<views::View> overlay_view_;
  // Pointer to the web view within the overlay view if it exists.
  raw_ptr<views::WebView> overlay_web_view_;

  // Preselection toast bubble. Weak; owns itself. NULL when closed.
  raw_ptr<views::Widget> preselection_widget_ = nullptr;

  // --------------------Browser window scoped state: END---------------------

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
