// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/lens/lens_overlay_translate_options.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/menu_model.h"
#include "ui/menus/simple_menu_model.h"

class GURL;
class LensOverlayController;
class LensOverlaySidePanelWebView;
class SidePanelEntryScope;
class SidePanelCoordinator;

enum class SidePanelEntryHideReason;

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
}  // namespace views

namespace lens {

class LensOverlaySidePanelNavigationThrottle;
class LensSearchboxController;

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
  std::optional<lens::TranslateOptions> translate_options_;
};

// Handles the creation and registration of the lens overlay side panel entry.
// There are two ways for this instance to be torn down.
//   (1) Its owner, LensOverlayController can destroy it.
//   (2) `side_panel_web_view_` can be destroyed by the side panel.
// In the case of (2), this instance is no longer functional and needs to be
// torn down. There are two constraints:
//   (2a) The shutdown path of LensOverlayController must be asynchronous. This
//   avoids re-entrancy into the code that is in turn calling (2).
//   (2b) Clearing local state associated with `side_panel_web_view_` must be
//   done synchronously.
class LensOverlaySidePanelCoordinator
    : public SidePanelEntryObserver,
      public lens::mojom::LensSidePanelPageHandler,
      public content::WebContentsObserver,
      public ChromeWebModalDialogManagerDelegate,
      public ui::SimpleMenuModel::Delegate {
 public:
  explicit LensOverlaySidePanelCoordinator(
      LensSearchController* lens_search_controller);
  LensOverlaySidePanelCoordinator(const LensOverlaySidePanelCoordinator&) =
      delete;
  LensOverlaySidePanelCoordinator& operator=(
      const LensOverlaySidePanelCoordinator&) = delete;
  ~LensOverlaySidePanelCoordinator() override;

  // Registers the side panel entry in the side panel if it doesn't already
  // exist and then shows it.
  void RegisterEntryAndShow();

  // Cleans up the side panel entry and closes the side panel.
  void DeregisterEntryAndCleanup();

  // SidePanelEntryObserver:
  void OnEntryWillHide(SidePanelEntry* entry,
                       SidePanelEntryHideReason reason) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Called by the destructor of the side panel web view.
  void WebViewClosing();

  content::WebContents* GetSidePanelWebContents();

  // Return the LensSearchController that owns this side panel coordinator.
  LensSearchController* GetLensSearchController() {
    return lens_search_controller_.get();
  }

  // Return the LensOverlayController that is part of this tab.
  LensOverlayController* GetLensOverlayController() {
    return lens_search_controller_->lens_overlay_controller();
  }

  // Return the LensSearchboxController that is part of this tab.
  LensSearchboxController* GetLensSearchboxController() {
    return lens_search_controller_->lens_searchbox_controller();
  }

  // Handles rendering text highlights on the main browser window based on
  // navigations from the side panel. Returns true if handled, false otherwise.
  // `nav_url` refers to the URL that the side panel was set to navigate to. It
  // is compared to the URL of the current open tab.
  bool MaybeHandleTextDirectives(const GURL& nav_url);

  // Whether the lens overlay entry is currently the active entry in the side
  // panel UI.
  bool IsEntryShowing();

  // Notifies the controller that a new query has been loaded in the side panel
  // results frame. This is used to adjust the overlay UI and
  // add the query to the history stack.
  void NotifyNewQueryLoaded(std::string query, GURL search_url);

  // lens::mojom::LensSidePanelPageHandler overrides.
  void PopAndLoadQueryFromHistory() override;
  void GetIsContextualSearchbox(
      GetIsContextualSearchboxCallback callback) override;
  void OnScrollToMessage(const std::vector<std::string>& text_fragments,
      uint32_t pdf_page_number) override;
  void RequestSendFeedback() override;

  // This method is used to set up communication between this instance and the
  // side panel WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  void BindSidePanel(
      mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensSidePanelPage> page);

  enum CommandID {
    COMMAND_MY_ACTIVITY,
    COMMAND_LEARN_MORE,
    COMMAND_SEND_FEEDBACK,
  };

  // SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // Show or hide the protected error page based on the value of
  // `show_protected_error_page.
  void SetShowProtectedErrorPage(bool show_protected_error_page);

  // Whether the side panel is currently showing the protected error page.
  bool IsShowingProtectedErrorPage();

  // Sets the latest page URL that was sent from the browser to the server. This
  // is currently only set when the latest page URL is a local file scheme URL
  // (`file://`). This is used to determine whether to scroll in the main tab or
  // open a new tab.
  void SetLatestPageUrlWithResponse(const GURL& url);

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. This is the state when the side panel is not
    // registered or shown. Currently, the only way to get back to the kOff
    // state is to destroy this class.
    kOff,

    // Opening the side panel.
    kOpeningSidePanel,

    // The side panel is open and the WebUI has been bound.
    kOpen,

    // TODO(crbug.com/335516480): Implement suspended state.
    kSuspended,
  };
  State state() { return state_; }

  // Suppresses the ghost loader in the side panel.
  void SuppressGhostLoader();

  /////////////////////////////////////////////////////////////////////////////
  // Test only methods.
  /////////////////////////////////////////////////////////////////////////////
  void LoadURLInResultsFrameForTesting(const GURL& url) {
    LoadURLInResultsFrame(url);
  }

  const std::vector<SearchQuery>& get_search_query_history_for_testing() {
    return initialization_data_->search_query_history_stack_;
  }

  const std::optional<SearchQuery>& get_loaded_search_query_for_testing() {
    return initialization_data_->currently_loaded_search_query_;
  }

  friend class ::LensOverlayController;
  friend class lens::LensOverlaySidePanelNavigationThrottle;

 protected:
  // Returns whether the side panel is bound to the WebUI.
  bool IsSidePanelBound();

  // Pass a result frame URL to load in the side panel.
  void LoadURLInResultsFrame(const GURL& url);

  // Notifies the side panel WebUI that the page content type has changed.
  void NotifyPageContentUpdated();

  // Sets whether the side panel should show a full error page. This is only
  // done if the side panel is not already in the state provided by the
  // parameters or on its first load.
  void MaybeSetSidePanelShowErrorPage(bool should_show_error_page,
                                      mojom::SidePanelResultStatus status);

  // Set the side panel state as being offline.
  void SetSidePanelIsOffline(bool is_offline);

  // Sets whether the results frame should show its loading state.
  virtual void SetSidePanelIsLoadingResults(bool is_loading);

  // Sets whether the side panel should show the back arrow.
  void SetBackArrowVisible(bool visible);

  // Sets the page content upload progress for the progress bar in the side
  // panel.
  void SetPageContentUploadProgress(double progress);

 private:
  // Data class for constructing the side panel and storing side panel state for
  // kSuspended state.
  struct SidePanelInitializationData {
   public:
    // This is data used to initialize the side panel after the WebUI has been
    // bound to the this class.
    SidePanelInitializationData();
    ~SidePanelInitializationData();
    // A list representing the search query stack that hosts the history of the
    // SRPs the user has navigated to.
    std::vector<SearchQuery> search_query_history_stack_;

    // The search query that is currently loaded in the results frame.
    std::optional<SearchQuery> currently_loaded_search_query_;
  };

  // content::WebContentsObserver:
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  // ChromeWebModalDialogManagerDelegate:
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // Whether the side panel should handle the URL differently since it has a
  // text directive and a URL that matches the current page. `nav_url` refers to
  // the URL that the side panel was set to navigate to. It is compared to the
  // URL of the current open tab.
  bool ShouldHandleTextDirectives(const GURL& nav_url);

  // Whether the side panel should send an extension event to update the
  // viewport since the URL being navigated to corresponds to the URL on the
  // current page and may contain viewport parameters to be parsed.
  bool ShouldHandlePDFViewportChange(const GURL& nav_url);

  // Callback for when the `text_finder` identifies the provided text directives
  // on the page. If all the directives were found, then this function will
  // create highlights on the page for each. Otherwise, it will open the
  // `nav_url` in a new tab.
  void OnTextFinderLookupComplete(
      const GURL& nav_url,
      const std::vector<std::pair<std::string, bool>>& lookup_results);

  // Opens the provided url params in the main browser as a new tab.
  void OpenURLInBrowser(const content::OpenURLParams& params);

  // Registers the entry in the side panel if it doesn't already exist.
  void RegisterEntry();

  // Record the error page being hidden / shown and set the value on the WebUI.
  void RecordAndShowSidePanelErrorPage();

  // Sets the URL to be used when opening the side panel in new tab.
  void SetSidePanelNewTabUrl(const GURL& url);

  // Gets the URL (with param modifications) to be used when opening the side
  // panel in new tab.
  GURL GetSidePanelNewTabUrl();

  // Shows a toast in the side panel with the string provided in `message`. If
  // the side panel connection has not been established or was reset this is a
  // no-op.
  void ShowToast(std::string message);

  // Called to get the URL for the "open in new tab" button.
  GURL GetOpenInNewTabUrl();

  std::unique_ptr<views::View> CreateLensOverlayResultsView(
      SidePanelEntryScope& scope);

  // Returns the more info callback for creating the side panel entry.
  base::RepeatingCallback<std::unique_ptr<ui::MenuModel>()>
  GetMoreInfoCallback();

  // Returns the menu model for the more info menu.
  std::unique_ptr<ui::MenuModel> GetMoreInfoMenuModel();

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // Connections to and from the side panel WebUI. Only valid when the side
  // panel is currently open and after the WebUI has started executing JS and
  // has bound the connection.
  mojo::Receiver<lens::mojom::LensSidePanelPageHandler> side_panel_receiver_{
      this};
  mojo::Remote<lens::mojom::LensSidePanelPage> side_panel_page_;

  // The assembly data needed for the side panel entry to be created and shown.
  std::unique_ptr<SidePanelInitializationData> initialization_data_;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // A pending url to be loaded in the side panel. Needed when the side
  // panel is not yet bound at the time of a request.
  std::optional<GURL> pending_side_panel_url_ = std::nullopt;

  // Whether the side panel should show the error page.
  bool side_panel_should_show_error_page_ = false;

  // URL to load when command to open side panel in a new tab is executed.
  GURL side_panel_new_tab_url_;

  // The latest page URL that was sent from the browser to the server. This is
  // currently only set when the latest page URL is a local file scheme URL
  // (`file://`).
  GURL latest_page_url_with_response_;

  // The status of the side panel, or whether it is currently showing an error
  // page.
  mojom::SidePanelResultStatus side_panel_result_status_ =
      mojom::SidePanelResultStatus::kUnknown;

  // General side panel coordinator responsible for all side panel interactions.
  // Separate from this class because this controls interactions to other side
  // panels as well, not just the Lens results. The side_panel_coordinator
  // lives with the browser view, so it should outlive this class. Therefore
  // this can be assumed to be non-null.
  raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;

  raw_ptr<LensOverlaySidePanelWebView> side_panel_web_view_;
  base::WeakPtrFactory<LensOverlaySidePanelCoordinator> weak_ptr_factory_{this};
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
