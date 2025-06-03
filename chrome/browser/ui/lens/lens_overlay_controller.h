// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/content_extraction/inner_html.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/page_content_type.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "chrome/browser/ui/lens/lens_overlay_blur_layer_delegate.h"
#include "chrome/browser/ui/lens/lens_overlay_colors.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_languages_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_translate_options.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_first_interaction_type.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_matcher/regex_set_matcher.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
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

namespace content {
class WebUI;
}  // namespace content

namespace lens {
class LensSessionMetricsLogger;
class LensOverlayQueryController;
class LensOverlaySidePanelCoordinator;
class LensPermissionBubbleController;
class LensSearchboxController;
class LensSearchContextualizationController;
struct SearchQuery;
class SidePanelInUse;
}  // namespace lens

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ui {
class TrackedElement;
}  // namespace ui

namespace variations {
class VariationsClient;
}  // namespace variations

namespace views {
class View;
class WebView;
}  // namespace views

class PrefService;
class Profile;
class LensSearchController;
enum class SidePanelEntryHideReason;

extern void* kLensOverlayPreselectionWidgetIdentifier;

// Manages all state associated with the lens overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class LensOverlayController : public lens::mojom::LensPageHandler,
                              public content::WebContentsDelegate,
                              public FullscreenObserver,
                              public views::ViewObserver,
                              public views::WidgetObserver,
                              public OmniboxTabHelper::Observer,
                              public content::RenderProcessHostObserver,
                              public ImmersiveModeController::Observer,
                              public find_in_page::FindResultObserver {
 public:
  LensOverlayController(tabs::TabInterface* tab,
                        LensSearchController* lens_search_controller,
                        variations::VariationsClient* variations_client,
                        signin::IdentityManager* identity_manager,
                        PrefService* pref_service,
                        syncer::SyncService* sync_service,
                        ThemeService* theme_service);
  ~LensOverlayController() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  // A simple utility that gets the the LensOverlayController TabFeature set by
  // the embedding tab of a lens WebUI hosted in `webui_web_contents`.
  // May return nullptr if no LensOverlayController TabFeature is associated
  // with `webui_web_contents`.
  static LensOverlayController* FromWebUIWebContents(
      content::WebContents* webui_web_contents);

  // A simple utility that gets the the LensOverlayController TabFeature set by
  // the instances of WebContents associated with a tab.
  // May return nullptr if no LensOverlayController TabFeature is associated
  // with `tab_web_contents`.
  static LensOverlayController* FromTabWebContents(
      content::WebContents* tab_web_contents);

  // This method is used to set up communication between this instance and the
  // overlay WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  virtual void BindOverlay(
      mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensPage> page);

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. There should be no performance overhead as
    // this state will apply to all tabs.
    kOff,

    // Waiting for reflow after closing side panel before taking a full page
    // screenshot.
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

    // Will be kOff soon.
    kClosing,
  };
  State state() { return state_; }

  // Returns the screenshot initially displayed on this overlay. If no
  // screenshot is showing, will return nullptr.
  const SkBitmap& initial_screenshot() {
    return initialization_data_->initial_screenshot_;
  }

  // Returns the screenshot of the live page which may have been updated after
  // the overlay is hidden and the live page is shown. If no screenshot is
  // showing, will return nullptr.
  const SkBitmap& updated_screenshot() {
    return initialization_data_->updated_screenshot_;
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

  // Returns invocation time since epoch. Used to set up html source for metric
  // logging.
  uint64_t GetInvocationTimeSinceEpoch();

  // Testing helper method for checking view housing our overlay.
  views::View* GetOverlayViewForTesting();

  // Testing helper method for checking web view.
  views::WebView* GetOverlayWebViewForTesting();

  // Send text data to the WebUI, or stores it to be sent when the WebUI is
  // ready.
  void SendText(lens::mojom::TextPtr text);

  // Creates theme with data obtained from `palette_id` to be sent to the WebUI.
  lens::mojom::OverlayThemePtr CreateTheme(lens::PaletteId palette_id);

  // Send overlay object data to the WebUI, or stores it to be sent when the
  // WebUI is ready.
  void SendObjects(std::vector<lens::mojom::OverlayObjectPtr> objects);

  // Send message to overlay notifying that the results side panel opened.
  void NotifyResultsPanelOpened();

  // Send message to overlay to copy the currently selection if any.
  void TriggerCopy();

  // Returns true if the overlay is open and covering the current active tab.
  bool IsOverlayShowing() const;

  // Returns true if the overlay is showing or is in live page mode.
  bool IsOverlayActive() const;

  // Returns true if the overlay is in the process of initializing.
  bool IsOverlayInitializing();

  // Returns true if the overlay is currently in the process of closing.
  bool IsOverlayClosing();

  // Pass a result frame URL to load in the side panel.
  void LoadURLInResultsFrame(const GURL& url);

  // Whether it's possible to capture a screenshot. virtual for testing.
  virtual bool IsScreenshotPossible(content::RenderWidgetHostView* view);

  // Returns the tab interface that that owns the search controller that owns
  // this overlay controller.
  tabs::TabInterface* GetTabInterface();

  // Show preselection toast bubble. Creates a preselection bubble if it does
  // not exist.
  void ShowPreselectionBubble();

  // Closes the preselection bubble and reopens it. Used to prevent UI conflicts
  // between the preselection bubble and top chrome in fullscreen.
  void CloseAndReshowPreselectionBubble();

  // Hides preselection toast bubble. Used when backgrounding the overlay. This
  // hides the widget associated with the bubble.
  void HidePreselectionBubble();

  // Queues a tutorial IPH to be shown if the given URL is eligible. Cancels any
  // queued IPH.
  void MaybeShowDelayedTutorialIPH(const GURL& url);

  // Updates the metrics related to navigations for the current page.
  void UpdateNavigationMetrics();

  // Testing function to issue a Lens region selection request.
  void IssueLensRegionRequestForTesting(lens::mojom::CenterRotatedBoxPtr region,
                                        bool is_click);

  // Testing function to issue a text request.
  void IssueTextSelectionRequestForTesting(const std::string& text_query,
                                           int selection_start_index,
                                           int selection_end_index,
                                           bool is_translate = false);

  // Testing function to issue a task completion event for a user action.
  void RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction user_action);

  // Testing function to issue a semantic event.
  void RecordSemanticEventForTesting(lens::mojom::SemanticEvent event);

  // Testing function to issue a translate request.
  void IssueTranslateSelectionRequestForTesting(
      const std::string& text_query,
      const std::string& content_language,
      int selection_start_index,
      int selection_end_index);

  // Testing function to issue a math request.
  void IssueMathSelectionRequestForTesting(const std::string& query,
                                           const std::string& formula,
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
      base::Time query_start_time,
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

  // Handles the event where searchbox was focused for testing.
  void OnFocusChangedForTesting(bool focused);

  // Handles the event where zero suggest was shown for testing.
  void OnZeroSuggestShownForTesting();

  // Opens the side panel for testing. If the side panel is already open, this
  // does nothing.
  void OpenSidePanelForTesting();

  // Returns the lens suggest inputs stored in this controller for testing.
  const lens::proto::LensOverlaySuggestInputs& GetLensSuggestInputsForTesting();

  // Returns true if tutorial IPH is eligible to be shown for the given URL for
  // testing.
  bool IsUrlEligibleForTutorialIPHForTesting(const GURL& url);

  const lens::mojom::CenterRotatedBoxPtr& get_selected_region_for_testing() {
    return initialization_data_->selected_region_;
  }

  const std::optional<std::pair<int, int>> get_selected_text_for_region() {
    return initialization_data_->selected_text_;
  }

  const std::vector<lens::mojom::CenterRotatedBoxPtr>&
  GetSignificantRegionBoxesForTesting() {
    return initialization_data_->significant_region_boxes_;
  }

  views::Widget* get_preselection_widget_for_testing() {
    return preselection_widget_.get();
  }

  lens::LensOverlayQueryController*
  get_lens_overlay_query_controller_for_testing() {
    return lens_overlay_query_controller_.get();
  }

 protected:
  friend class LensSearchController;
  friend class lens::LensSearchboxController;
  friend class lens::LensOverlaySidePanelCoordinator;

  // This is entry point for showing the overlay UI. This has no effect if state
  // is not kOff. This has no effect if the tab is not in the foreground. If the
  // overlay is successfully invoked, then the value of `invocation_source` will
  // be recorded in the relevant metrics.
  void ShowUI(lens::LensOverlayInvocationSource invocation_sourc,
              lens::LensOverlayQueryController* lens_overlay_query_controller);

  // Issues a contextual search request for Lens to fulfill.
  // No-op if the Lens Overlay is off or closing. If the Lens Overlay is in the
  // process of opening, the request will be queued until the overlay is fully
  // opened.
  // TODO(crbug.com/403629222): Revisit if it makes sense to pass the
  // destination URL instead of the query text directly.
  void IssueContextualSearchRequest(
      const GURL& destination_url,
      lens::LensOverlayQueryController* lens_overlay_query_controller,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion,
      lens::LensOverlayInvocationSource invocation_source);

  // Sets a region to search after the overlay loads, then calls ShowUI().
  // All units are in device pixels. region_bitmap contains the high definition
  // image bytes to use for the search instead of cropping the region from the
  // viewport.
  void ShowUIWithPendingRegion(
      lens::LensOverlayQueryController* lens_overlay_query_controller,
      lens::LensOverlayInvocationSource invocation_source,
      lens::mojom::CenterRotatedBoxPtr region,
      const SkBitmap& region_bitmap);

  // Plays the overlay close animation and then invokes the callback.
  void TriggerOverlayCloseAnimation(base::OnceClosure callback);

  // Closes the overlay UI and sets state to kOff. This method is the final
  // cleanup of closing the overlay UI. This resets all state internal to the
  // LensOverlayController.
  // Anyone called trying to close the UI should go through CloseUIAsync or
  // CloseUISync. Those methods also reset state external to
  // LensOverlayController.
  void CloseUI(lens::LensOverlayDismissalSource dismissal_source);

  // Returns the vsrid to use for the new tab URL.
  std::string GetVsridForNewTab();

  // Sets the overlay translate mode. If `translate_options` is nullopt, it will
  // disable translate mode.
  void SetTranslateMode(
      std::optional<lens::TranslateOptions> translate_options);

  // Sets the text selection on the overlay.
  void SetTextSelection(int32_t selection_start_index,
                        int32_t selection_end_index);

  // Sets the post region selection on the overlay.
  void SetPostRegionSelection(lens::mojom::CenterRotatedBoxPtr);

  // Stores the additional query parameters to pass to the query controller for
  // generating urls, set by the search box.
  void SetAdditionalSearchQueryParams(
      std::map<std::string, std::string> additional_search_query_params);

  // Clears the selected text from the overlay if there is any.
  void ClearTextSelection();

  // Clears the selected region.
  void ClearRegionSelection();

  // Clears any selections currently made in the overlay.
  void ClearAllSelections();

  // Called by the searchbox controller when the focus on the searchbox changes.
  void OnSearchboxFocusChanged(bool focused);

  // Called by the searchbox controller when zero suggest is shown.
  void OnZeroSuggestShown();

  // Makes a Lens request and updates all state related to the Lens request. If
  // region_bitmap is provided, it will use those bytes to send to the Lens
  // server instead of cropping the region from the full page screenshot.
  void IssueLensRequest(base::Time query_start_time,
                        lens::mojom::CenterRotatedBoxPtr region,
                        lens::LensOverlaySelectionType selection_type,
                        std::optional<SkBitmap> region_bitmap);

  // Issues a multimodal request to the query controller.
  void IssueMultimodalRequest(base::Time query_start_time,
                              lens::mojom::CenterRotatedBoxPtr region,
                              const std::string& text_query,
                              lens::LensOverlaySelectionType selection_type,
                              std::optional<SkBitmap> region_bitmap);

  // Tries to update the page content and then issues a searchbox request.
  void IssueSearchBoxRequest(
      base::Time query_start_time,
      const std::string& search_box_text,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion,
      std::map<std::string, std::string> additional_query_params);

  // Issues a contextual text request to the query controller.
  void IssueContextualTextRequest(
      base::Time query_start_time,
      const std::string& text_query,
      lens::LensOverlaySelectionType selection_type);

  // Returns a search query struct containing the current state of the overlay.
  void AddOverlayStateToSearchQuery(lens::SearchQuery& search_query);

  // TODO(crbug.com/404941800): All the Handle*Response methods should not exist
  // in this class. They currently exist to unblock development. They will be
  // removed once the migration is complete. Handles the response to the Lens
  // start query request.
  void HandleStartQueryResponse(
      std::vector<lens::mojom::OverlayObjectPtr> objects,
      lens::mojom::TextPtr text,
      bool is_error);

  // Handles the URL response to the Lens interaction request.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Handles the text response to the Lens interaction request.
  void HandleInteractionResponse(lens::mojom::TextPtr text);

  // Handles the progress of the page content upload. Notifies the side panel
  // to update the progress bar.
  void HandlePageContentUploadProgress(uint64_t position, uint64_t total);

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
                              GURL page_url,
                              std::optional<std::string> page_title);
    ~OverlayInitializationData();

    // Whether there is any full image response data present.
    bool has_full_image_response() const {
      return !text_.is_null() || !objects_.empty();
    }

    // The screenshot that is initially rendered by the WebUI.
    // initial_screenshot_ is in native format and is needed to encode JPEGs to
    // send to the server. initial_rgb_screenshot_ is in RGBA color type and
    // used to display in the WebUI. initial_rgb_screenshot_ cannot be used to
    // encode JPEGs because the JPEG encoder expects the native color format.
    SkBitmap initial_screenshot_;
    SkBitmap initial_rgb_screenshot_;

    // Screenshot of the live page which may be updated after the overlay is
    // hidden and the live page is shown. Initially equal to
    // initial_screenshot_.
    SkBitmap updated_screenshot_;

    // The dynamic color palette identifier based on the screenshot.
    lens::PaletteId color_palette_;

    // The page url. Empty if it is not allowed to be shared.
    GURL page_url_;

    // The page title, if it is allowed to be shared.
    std::optional<std::string> page_title_;

    // The data of the content the user is viewing. There can be multiple
    // content types for a single page, so we store them all in this struct.
    std::vector<lens::PageContent> page_contents_;

    // The primary type of the data stored in page_contents_. This is the value
    // used to determine request params and what content to look at when
    // determining if the page_contents_ needs to be present.
    lens::MimeType primary_content_type_ = lens::MimeType::kUnknown;

    // The page count of the PDF document if page_content_type_ is kPdf.
    std::optional<uint32_t> pdf_page_count_;

    // The partial representation of a PDF document. The element at a given
    // index holds the text of the PDF page at the same index.
    std::vector<std::u16string> pdf_pages_text_;

    // The most visible page of the PDF document when the viewport was last
    // updated, if page_content_type_ is kPdf.
    std::optional<uint32_t> last_retrieved_most_visible_page_;

    // Bounding boxes for significant regions identified in the screenshot.
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes_;

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

    // The translate options currently enabled in the overlay.
    std::optional<lens::TranslateOptions> translate_options_;
  };

  class UnderlyingWebContentsObserver;

  // Takes a screenshot of the current viewport.
  void CaptureScreenshot();

  // Fetches the bounding boxes of all images within the current viewport.
  void FetchViewportImageBoundingBoxes(const SkBitmap& bitmap);

  // Gets the current page number if viewing a PDF.
  void GetPdfCurrentPage(
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
          chrome_render_frame,
      int attempt_id,
      const SkBitmap& bitmap,
      const std::vector<gfx::Rect>& bounds);

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
      const std::vector<gfx::Rect>& bounds,
      std::optional<uint32_t> pdf_current_page);

  // Process the bitmap and creates all necessary data to initialize the
  // overlay. Happens on a separate thread to prevent main thread from hanging.
  void CreateInitializationData(const SkBitmap& screenshot,
                                const std::vector<gfx::Rect>& all_bounds,
                                std::optional<uint32_t> pdf_current_page);

  // Called after creating the RGB bitmap and we are back on the main thread.
  void ContinueCreateInitializationData(
      const SkBitmap& screenshot,
      const std::vector<gfx::Rect>& all_bounds,
      std::optional<uint32_t> pdf_current_page,
      SkBitmap rgb_screenshot);

  // Stores the page content and continues the initialization process. Also
  // records the page count for PDF.
  void StorePageContentAndContinueInitialization(
      std::unique_ptr<OverlayInitializationData> initialization_data,
      std::vector<lens::PageContent> page_contents,
      lens::MimeType primary_content_type,
      std::optional<uint32_t> page_count);

  // Creates the mojo bounding boxes for the significant regions.
  std::vector<lens::mojom::CenterRotatedBoxPtr> ConvertSignificantRegionBoxes(
      const std::vector<gfx::Rect>& all_bounds);

  // Updates state of the ghost loader. |suppress_ghost_loader| is true when
  // the page bytes can't be uploaded.
  void SuppressGhostLoader();

  // Enables/disables the background blur updating live. This should be used to
  // save resources on blurring the background when not needed.
  void SetLiveBlur(bool enabled);

  // Called when the UI needs to show the overlay via a view that is a child of
  // the tab contents view.
  void ShowOverlay();

  // Hides the overlay view and restores input to the tab contents web view.
  void HideOverlay();

  // Hide the shared overlay view if it is not being used by another tab. This
  // is determined by checking if any of the children of the overlay view are
  // visible.
  void MaybeHideSharedOverlayView();

  // Requests to open the side panel if this class has not already done so.
  // Must be called before issuing results to the side panel.
  void MaybeOpenSidePanel();

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

  // Returns true if the searchbox is a CONTEXTUAL_SEARCHBOX.
  bool IsContextualSearchbox();

  // Called when the UI needs to create the view to show in the overlay.
  raw_ptr<views::View> CreateViewForOverlay();

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
#if BUILDFLAG(IS_MAC)
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
#endif
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

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;

  // Called when the Lens backend handshake is complete.
  void OnHandshakeComplete();

  // Gets the page classification from the searchbox controller.
  metrics::OmniboxEventProto::PageClassification GetPageClassification() const;

  // Adds a callback to be called when the Lens backend handshake is finished.
  // If the handshake is already finished, the callback will be called
  // immediately.
  void OnLensBackendHandshakeFinished(base::OnceClosure callback);

  // Gets the ui scale factor of the page.
  float GetUiScaleFactor();

  // Called anytime the side panel opens. Used to close lens overlay when
  // another side panel opens.
  void OnSidePanelDidOpen();

  // Called to continue the screenshot process while opening lens overlay.
  void FinishedWaitingForReflow();

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab will enter the background.
  void TabWillEnterBackground(tabs::TabInterface* tab);

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
                                 int selection_end_index,
                                 bool is_translate) override;
  void IssueTranslateFullPageRequest(
      const std::string& source_language,
      const std::string& target_language) override;
  void IssueEndTranslateModeRequest() override;
  void IssueTranslateSelectionRequest(const std::string& text_query,
                                      const std::string& content_language,
                                      int selection_start_index,
                                      int selection_end_index) override;
  void IssueMathSelectionRequest(const std::string& query,
                                 const std::string& formula,
                                 int selection_start_index,
                                 int selection_end_index) override;

  void NotifyOverlayInitialized() override;
  void RecordUkmAndTaskCompletionForLensOverlayInteraction(
      lens::mojom::UserAction user_action) override;
  void RecordLensOverlaySemanticEvent(
      lens::mojom::SemanticEvent event) override;
  void SaveAsImage(lens::mojom::CenterRotatedBoxPtr region) override;
  void MaybeShowTranslateFeaturePromo() override;
  void MaybeCloseTranslateFeaturePromo(bool feature_engaged) override;
  void FetchSupportedLanguages(
      FetchSupportedLanguagesCallback callback) override;

  // Tries to show the translate feature promo after the translate button
  // element is shown.
  void TryShowTranslateFeaturePromo(ui::TrackedElement* element);

  // Performs shared logic for IssueTextSelectionRequest() and
  // IssueTranslateSelectionRequest().
  void IssueTextSelectionRequestInner(base::Time query_start_time,
                                      const std::string& text_query,
                                      int selection_start_index,
                                      int selection_end_index);

  // Handles a request (either region or multimodal) trigger by sending
  // the request to the query controller.
  void IssueSearchBoxRequestPart2(
      base::Time query_start_time,
      const std::string& search_box_text,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion,
      std::map<std::string, std::string> additional_query_params);

  // Launches the Lens overlay HaTS survey if eligible.
  void MaybeLaunchSurvey();

  // Initialize the tutorial IPH URL matcher from finch config.
  void InitializeTutorialIPHUrlMatcher();

  // Returns true if tutorial IPH is eligible to be shown for the given URL.
  bool IsUrlEligibleForTutorialIPH(const GURL& url);

  // Shows the tutorial IPH.
  void ShowTutorialIPH();

  // Notifies the user education service that the overlay has been used.
  void NotifyUserEducationAboutOverlayUsed();

  // Notifies the overlay or side panel that the page content type has changed.
  void NotifyPageContentUpdated();

  // Notifies the entry point controller to update the state of the entry
  // points since the state of the overlay has changed.
  void UpdateEntryPointsState();

  // Callback to run when the partial page text is retrieved from the PDF.
  void OnPdfPartialPageTextRetrieved(
      std::vector<std::u16string> pdf_pages_text);

  // Callback to run when the page context has been updated and the suggestion
  // query should now be issued.
  void OnPageContextUpdatedForSuggestion(
      const GURL& destination_url,
      AutocompleteMatchType::Type match_type,
      bool is_zero_prefix_suggestion,
      lens::LensOverlayInvocationSource invocation_source);

  // Shorthand to grab the LensSearchboxController for this instance of Lens.
  lens::LensSearchboxController* GetLensSearchboxController();

  // Shorthand to grab the LensSearchContextualizationController for this
  // instance of Lens.
  lens::LensSearchContextualizationController* GetContextualizationController();

  // Shorthand to grab the LensSessionMetricsLogger for this instance of Lens.
  lens::LensSessionMetricsLogger* GetLensSessionMetricsLogger();

  // Owns the LensSearchController which owns this class
  raw_ptr<tabs::TabInterface> tab_;

  // Owns this class.
  raw_ptr<LensSearchController> lens_search_controller_;

  // A monotonically increasing id. This is used to differentiate between
  // different screenshot attempts.
  int screenshot_attempt_id_ = 0;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // Tracks the state of the overlay when it is backgrounded. This is the state
  // that the overlay will return to when the tab is foregrounded.
  State backgrounded_state_ = State::kOff;

  // The assembly data needed for the overlay to be created and shown.
  std::unique_ptr<OverlayInitializationData> initialization_data_;

  // Invocation source for the lens overlay.
  lens::LensOverlayInvocationSource invocation_source_ =
      lens::LensOverlayInvocationSource::kAppMenu;

  // A contextual search request to be issued once the overlay is initialized.
  base::OnceClosure pending_contextual_search_request_;

  // Pending region to search after the overlay loads.
  lens::mojom::CenterRotatedBoxPtr pending_region_;

  // The bitmap for the pending region stored in pending_region_.
  // pending_region_ and pending_region_bitmap_ are correlated and their
  // lifecycles are should stay in sync.
  SkBitmap pending_region_bitmap_;

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

  // Observer for the WebContents of the associated tab. Only valid while the
  // overlay view is showing.
  std::unique_ptr<UnderlyingWebContentsObserver> tab_contents_observer_;

  // Query controller. Owned by the search controller, guaranteed to be alive
  // until the overlay is closed.
  raw_ptr<lens::LensOverlayQueryController> lens_overlay_query_controller_;

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

  // Whether the OCR DOM similarity has been recorded in the current session.
  bool ocr_dom_similarity_recorded_in_session_ = false;
  // The time at which the overlay was invoked. Used to compute timing metrics.
  base::TimeTicks invocation_time_;

  // The time at which the overlay was invoked, since epoch. Used to calculate
  // timeToWebUIReady on the WebUI side.
  base::Time invocation_time_since_epoch_;

  // Indicates whether this is the first upload handler event received. This is
  // used to determine whether to show the upload progress bar.
  bool is_first_upload_handler_event_ = true;

  // Indicates whether the upload progress bar is currently being shown for this
  // upload.
  bool is_upload_progress_bar_shown_ = true;

  // Indicates whether the user is currently on a context eligible page.
  bool is_page_context_eligible_ = true;

  // Indicates whether the screenshot should be sent when updating the page
  // content when first initializing the overlay. This is only used when the
  // early start query flow optimization is enabled. Setting this to true does
  // not guarantee the screenshot is sent on initialization, as that is still
  // dependent on whether the page is context eligible or not.
  bool should_send_screenshot_on_init_ = false;

  // TODO(384778180): The two `pre_initialization_*` fields below are used to
  // store data that came back before the initialization data was ready. This
  // should be refactored into one struct to make it cleaner.
  //
  // The stored objects response to be attached to the initialization data
  // if the object response came back before the initialization data was ready.
  std::optional<std::vector<lens::mojom::OverlayObjectPtr>>
      pre_initialization_objects_;

  // The stored text response to be attached to the initialization data
  // if the text response came back before the initialization data was ready.
  std::optional<lens::mojom::TextPtr> pre_initialization_text_;

  // The callback subscription for the element shown callback used to show the
  // translate feature promo.
  base::CallbackListSubscription translate_button_shown_subscription_;

  // Matcher for URLs that are eligible to have the tutorial IPH shown.
  std::unique_ptr<url_matcher::URLMatcher> tutorial_iph_url_matcher_;

  // Matcher for URLs that are do not need to pass the check for allowed paths.
  // Instead, if they match the tutorial_iph_url_matcher_` and do not contain
  // any of the blocked paths, they are considered matches.
  std::unique_ptr<url_matcher::RegexSetMatcher> forced_url_matcher_;

  // Matcher for URL paths that are eligible to have the tutorial IPH shown.
  std::unique_ptr<url_matcher::RegexSetMatcher> page_path_allow_matcher_;

  // Matcher for URL paths that are not eligible to have the tutorial IPH shown.
  std::unique_ptr<url_matcher::RegexSetMatcher> page_path_block_matcher_;

  // Filters used by the URL matcher. Used to look up if a matching filter is an
  // allow filter or a block filter.
  std::map<base::MatcherStringPattern::ID, url_matcher::util::FilterComponents>
      iph_url_filters_;

  // Used to cancel showing a queued tutorial IPH.
  base::OneShotTimer tutorial_iph_timer_;

  // ---------------Browser window scoped state: START---------------------
  // State that is scoped to the browser window must be reset when the tab is
  // backgrounded, since the tab may move between browser windows.

  // Observes the side panel of the browser window.
  base::CallbackListSubscription side_panel_shown_subscription_;

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

  // Observer to get notifications when the immersive mode reveal state changes.
  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observer_{this};

  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_tab_helper_observer_{this};

  // The controller for sending gen204 pings. Owned by the overlay controller
  // so that the life cycle outlasts the query controller, allowing gen204
  // requests to be sent upon query end.
  std::unique_ptr<lens::LensOverlayGen204Controller> gen204_controller_;

  // The controller for sending requests to get the list of supported languages.
  // Requests are only made if the WebUI has not already cached the languages
  // and none of the update cache conditions are met.
  std::unique_ptr<lens::LensOverlayLanguagesController> languages_controller_;

  // General side panel coordinator responsible for all side panel interactions.
  // Separate from the results_side_panel_coordinator because this controls
  // interactions to other side panels as well, not just our results. The
  // side_panel_coordinator lives with the browser view, so it should outlive
  // this class. Therefore, if the controller is not in the kOff state, this can
  // be assumed to be non-null.
  raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;

  // Side panel coordinator for the side panel coordinator that controls the
  // results side panel. Guaranteed to exist if the overlay is not `kOff`.
  raw_ptr<lens::LensOverlaySidePanelCoordinator>
      results_side_panel_coordinator_;

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

  // The anchor view to the preselection bubble. This anchor is an invisible
  // sibling of the the `overlay_view_`, user to always keep the preselection
  // bubble anchored to the top of the screen, while also maintaining focus
  // order.
  raw_ptr<views::View> preselection_widget_anchor_;

#if BUILDFLAG(IS_MAC)
  // Register for adding observers to prefs the current profiles pref service.
  // Currently only used to observe the immersive mode pref on Mac.
  PrefChangeRegistrar pref_change_registrar_;
#endif  // BUILDFLAG(IS_MAC)

  // --------------------Browser window scoped state: END---------------------

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
