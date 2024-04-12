// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/resources/lens/server/proto/lens_overlay_response.pb.h"
#include "chrome/browser/ui/tabs/tab_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/unique_widget_ptr.h"

class TabStripModel;

namespace lens {
class LensOverlaySidePanelCoordinator;
class LensOverlayQueryController;
}  // namespace lens

namespace tabs {
class TabModel;
}  // namespace tabs

namespace views {
class View;
class WebView;
}  // namespace views

namespace content {
class WebUI;
}  // namespace content

// Manages all state associated with the lens overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class LensOverlayController : public TabStripModelObserver,
                              public LensSearchboxClient,
                              public lens::mojom::LensPageHandler,
                              public lens::mojom::LensSidePanelPageHandler,
                              public tabs::TabModelObserver {
 public:
  explicit LensOverlayController(tabs::TabModel* tab_model);
  ~LensOverlayController() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlayId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOverlaySidePanelWebViewId);

  // Returns whether the lens overlay feature is enabled. This value is
  // guaranteed not to change over the lifetime of a LensOverlayController.
  bool Enabled();

  // This is entry point for showing the overlay UI. This has no effect if state
  // is not kOff. This has no effect if the tab is not in the foreground.
  void ShowUI();

  // Closes the overlay UI and sets state to kOff. This method should be
  // idempotent. This synchronously destroys any associated WebUIs, so should
  // not be invoked in callbacks from those WebUIs.
  void CloseUI();

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

    // Will be kOff soon.
    kClosing,
  };
  State state() { return state_; }

  // Returns the screenshot currently being displayed on this overlay. If no
  // screenshot is showing, will return nullptr.
  const SkBitmap& current_screenshot() { return current_screenshot_; }

  // Returns the side panel coordinator
  lens::LensOverlaySidePanelCoordinator* side_panel_coordinator() {
    return results_side_panel_coordinator_.get();
  }

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

  // Returns true if the overlay is open and covering the current active tab.
  bool IsOverlayShowing();

  // Handles the response to the Lens start query request.
  void HandleStartQueryResponse(
      std::vector<lens::mojom::OverlayObjectPtr> objects,
      lens::mojom::TextPtr text);

  // Handles when the side panel has been deregistered to do any required
  // cleanup.
  void OnSidePanelEntryDeregistered();

  // Testing function to issue a text request.
  // TODO(b/328294794): Remove this function when connecting the mojo call.
  void IssueTextSelectionRequestForTesting(const std::string& text_query);

 private:
  class UnderlyingWebContentsObserver;

  // Called once a screenshot has been captured. This should trigger transition
  // to kOverlay. As this process is asynchronous, there are edge cases that can
  // result in multiple in-flight screenshot attempts. We record the
  // `attempt_id` for each attempt so we can ignore all but the most recent
  // attempt.
  void DidCaptureScreenshot(int attempt_id, const SkBitmap& bitmap);

  // Called when the UI needs to create the overlay widget.
  void ShowOverlayWidget();

  // Creates InitParams for the overlay widget based on the window bounds.
  views::Widget::InitParams CreateWidgetInitParams();

  // Called when the UI needs to create the view to show in the overlay.
  std::unique_ptr<views::View> CreateViewForOverlay();

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Overridden from LensSearchboxClient:
  const GURL& GetPageURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override;
  const std::string& GetThumbnail() const override;
  const lens::LensOverlayInteractionResponse& GetLensResponse() const override;
  void OnThumbnailRemoved() const override;
  void OnSuggestionAccepted(const GURL& destination_url) override;

  // Called when the associated tab enters the foreground.
  void TabForegrounded();

  // Called when the associated tab enters the background.
  void TabBackgrounded();

  // lens::mojom::LensPageHandler overrides.
  void CloseRequestedByOverlay() override;
  // TODO: rename this to IssueRegionSearchRequest.
  void IssueLensRequest(lens::mojom::CenterRotatedBoxPtr region) override;

  // Handles an object selection by sending the request to the query
  // controller.
  void IssueObjectSelectionRequest(const std::string& object_id);

  // Handles a text selection by sending a text-only request to the query
  // controller and to the search box.
  void IssueTextSelectionRequest(const std::string& text_query);

  // Calls CloseUI() asynchronously.
  void CloseUIAsync();

  // Handles the URL response to the Lens interaction request.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Handles the suggest signals response to the Lens interaction request.
  void HandleInteractionDataResponse(
      lens::proto::LensOverlayInteractionResponse response);

  // tabs::TabModelObserver overrides:
  void WillRemoveContents(tabs::TabModel* tab,
                          content::WebContents* contents) override;
  void DidAddContents(tabs::TabModel* tab,
                      content::WebContents* contents) override;

  // Owns this class.
  raw_ptr<tabs::TabModel> tab_model_;

  // A monotonically increasing id. This is used to differentiate between
  // different screenshot attempts.
  int screenshot_attempt_id_ = 0;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // Pointer to the overlay widget.
  views::UniqueWidgetPtr overlay_widget_;

  // Pointer to the WebViews that are being glued by this class. Only used to
  // clean up stale pointers. Only valid while `overlay_widget_` is showing.
  std::vector<views::WebView*> glued_webviews_;

  // The screenshot that is currently being rendered by the WebUI.
  SkBitmap current_screenshot_;

  // A pending url to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<GURL> pending_side_panel_url_ = std::nullopt;

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

  // Searchbox handler for passing in image and text selections.
  std::unique_ptr<RealboxHandler> searchbox_handler_;

  // Observer for the WebContents of the associated tab. Only valid while the
  // overlay widget is showing.
  std::unique_ptr<UnderlyingWebContentsObserver> tab_contents_observer_;

  // Query controller.
  std::unique_ptr<lens::LensOverlayQueryController>
      lens_overlay_query_controller_;

  // The selected region. Stored so that it can be used for multiple
  // requests, such as if the user changes the text query without changing
  // the region. Cleared if the user makes a text-only or object selection
  // query.
  lens::mojom::CenterRotatedBoxPtr selected_region_;

  base::ScopedObservation<tabs::TabModel, tabs::TabModelObserver>
      tab_model_observer_{this};

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
