// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_query_controller.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace lens {
class LensOverlaySidePanelCoordinator;
class LensOverlayQueryController;
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

namespace variations {
class VariationsClient;
}  // namespace variations

// Manages all state associated with the lens overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class LensOverlayController : public LensSearchboxClient,
                              public lens::mojom::LensPageHandler,
                              public lens::mojom::LensSidePanelPageHandler {
 public:
  LensOverlayController(tabs::TabInterface* tab,
                        variations::VariationsClient* variations_client,
                        signin::IdentityManager* identity_manager);
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
  void IssueTextSelectionRequestForTesting(const std::string& text_query);

  // Gets the WebContents housed in the side panel for testing.
  content::WebContents* GetSidePanelWebContentsForTesting();

 protected:
  // Override these methods to stub out network requests for testing.
  virtual std::unique_ptr<lens::LensOverlayQueryController>
  CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_data_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager);

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

  // Overridden from LensSearchboxClient:
  const GURL& GetPageURL() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override;
  const std::string& GetThumbnail() const override;
  const lens::proto::LensOverlayInteractionResponse& GetLensResponse()
      const override;
  void OnThumbnailRemoved() const override;
  void OnSuggestionAccepted(const GURL& destination_url) override;
  void OnPageBound() override;

  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);

  // Called when the associated tab enters the background.
  void TabBackgrounded(tabs::TabInterface* tab);

  // Called when the tab's WebContents are removed.
  void WillRemoveContents(tabs::TabInterface* tab,
                          content::WebContents* contents);

  // Called when the tab's WebContents are added.
  void DidAddContents(tabs::TabInterface* tab, content::WebContents* contents);

  // lens::mojom::LensPageHandler overrides.
  void CloseRequestedByOverlay() override;
  // TODO: rename this to IssueRegionSearchRequest.
  void IssueLensRequest(lens::mojom::CenterRotatedBoxPtr region) override;

  // Handles an object selection by sending the request to the query
  // controller.
  void IssueObjectSelectionRequest(const std::string& object_id);

  // Handles a text selection by sending a text-only request to the query
  // controller and to the search box.
  void IssueTextSelectionRequest(const std::string& text_query) override;

  // Calls CloseUI() asynchronously.
  void CloseUIAsync();

  // Handles the URL response to the Lens interaction request.
  void HandleInteractionURLResponse(
      lens::proto::LensOverlayUrlResponse response);

  // Handles the suggest signals response to the Lens interaction request.
  void HandleInteractionDataResponse(
      lens::proto::LensOverlayInteractionResponse response);

  // Owns this class.
  raw_ptr<tabs::TabInterface> tab_;

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
  std::string current_screenshot_data_uri_;

  // A pending url to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<GURL> pending_side_panel_url_ = std::nullopt;

  // A pending text query to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<std::string> pending_text_query_ = std::nullopt;

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

  // The selected region. Stored so that it can be used for multiple
  // requests, such as if the user changes the text query without changing
  // the region. Cleared if the user makes a text-only or object selection
  // query.
  lens::mojom::CenterRotatedBoxPtr selected_region_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  raw_ptr<signin::IdentityManager> identity_manager_;

  // Prevents other features from showing tab-modal UI.
  std::unique_ptr<tabs::ScopedTabModalUI> scoped_tab_modal_ui_;

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
