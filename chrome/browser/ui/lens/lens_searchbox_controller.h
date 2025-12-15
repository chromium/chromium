// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_SEARCHBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_SEARCHBOX_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lens/core/mojom/lens_ghost_loader.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/ui/lens/lens_query_flow_router.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_client.h"
#include "chrome/browser/ui/webui/searchbox/lens_searchbox_handler.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

class LensSearchController;

using GetIsContextualSearchboxCallback =
    lens::mojom::LensSidePanelPageHandler::GetIsContextualSearchboxCallback;

namespace lens {

struct SearchQuery;
namespace proto {
class LensOverlaySuggestInputs;
}  // namespace proto

// Controller for the Lens searchbox. This class is responsible for handling
// communications between the Lens WebUI searchbox and other Lens components.
// This class is responsible for both the overlay and side panel searchboxes.
class LensSearchboxController : public LensSearchboxClient {
 public:
  explicit LensSearchboxController(
      LensSearchController* lens_search_controller);
  ~LensSearchboxController() override;

  // This method is used to set up communication between this instance and the
  // overlay ghost loader's WebUI. This is called by the WebUIController when
  // the WebUI is executing javascript and ready to bind.
  void BindOverlayGhostLoader(
      mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page);

  // This method is used to set up communication between this instance and the
  // side panel's ghost loader WebUI. This is called by the WebUIController
  // when the WebUI is executing javascript and ready to bind.
  void BindSidePanelGhostLoader(
      mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page);

  // Must be called at the start of a session so the proper state is
  // initialized. Optionally set whether to suppress contextualization for the
  // current session.
  void OnSessionStart(bool suppress_contextualization = false);

  // This method is used to set up communication between this instance and the
  // searchbox WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and has bound the handler. Takes ownership of
  // `handler`.
  void SetSidePanelSearchboxHandler(
      std::unique_ptr<LensSearchboxHandler> handler);

  // Passes ownership of the lens searchbox handler to the search bubble
  // controller. This is called by the WebUIController when the WebUI is
  // executing javascript and has bound the handler.
  void SetContextualSearchboxHandler(
      std::unique_ptr<LensSearchboxHandler> handler);

  // This method is used to release the owned `SearchboxHandler` for the
  // overlay. It should be called before the overlay web contents is destroyed
  // since it contains a reference to that web contents.
  void ResetOverlaySearchboxHandler();

  // This method is used to release the owned `SearchboxHandler`. It should be
  // called before the side panel web contents is destroyed since it contains a
  // reference to that web contents.
  void ResetSidePanelSearchboxHandler();

  // Sets the input text for the searchbox. If the searchbox has not been bound,
  // it stores it in `pending_text_query_` instead.
  void SetSearchboxInputText(const std::string& text);

  // Sets the thumbnail URI values on the searchbox if it is bound.
  void SetSearchboxThumbnail(const std::string& thumbnail_uri);

  // Sets whether the thumbnail is shown in the side panel.
  void SetShowSidePanelSearchboxThumbnail(bool shown);

  // Cleans up internal state associated with the searchbox.
  void CloseUI();

  // Gets whether this is currently a contextual searchbox.
  bool IsContextualSearchbox() const;

  // Gets whether this searchbox is currently in the side panel. False if it is
  // in the overlay.
  bool IsSidePanelSearchbox() const;

  // Returns whether the searchbox is in contextual mode by passing the result
  // of IsContextualSearchbox() to the callback.
  void GetIsContextualSearchbox(GetIsContextualSearchboxCallback callback);

  // Waits for the handshake with the Lens backend to complete and then invokes
  // the callback with the LensOverlaySuggestInputs. Callback will be invoked
  // immediately if the handshake is already complete.
  base::CallbackListSubscription GetLensSuggestInputsWhenReady(
      ::LensOverlaySuggestInputsCallback callback);

  // Called when the suggest inputs have been updated and are ready to be sent
  // to any pending callbacks.
  void NotifySuggestInputsReady(
      lens::proto::LensOverlaySuggestInputs suggest_inputs);

  // Overridden from LensSearchboxClient:
  const GURL& GetPageURL() const override;
  SessionID GetTabId() const override;
  metrics::OmniboxEventProto::PageClassification GetPageClassification()
      const override;
  std::string& GetThumbnail() override;
  lens::proto::LensOverlaySuggestInputs GetLensSuggestInputs() const override;
  void OnTextModified() override;
  void OnThumbnailRemoved() override;
  void OnSuggestionAccepted(const GURL& destination_url,
                            AutocompleteMatchType::Type match_type,
                            bool is_zero_prefix_suggestion) override;
  void OnFocusChanged(bool focused) override;
  void OnPageBound() override;
  void ShowGhostLoaderErrorState() override;
  void OnZeroSuggestShown() override;

  // Adds searchbox related state to the search query.
  void AddSearchboxStateToSearchQuery(lens::SearchQuery& search_query);

 private:
  // Data class for storing state for the searchbox.
  struct LensSearchboxInitializationData {
   public:
    LensSearchboxInitializationData();
    ~LensSearchboxInitializationData() = default;
    // The text query in the searchbox.
    std::string text_query = "";

    // The URI of the thumbnail in the searchbox.
    std::string thumbnail_uri = "";

    // Whether to suppress contextualization for the current session.
    bool suppress_contextualization = false;

    // Whether the thumbnail should be shown in the side panel.
    bool show_side_panel_thumbnail = true;
  };

  // Returns the WebContents associated with the tab this instance of Lens is
  // invoked on.
  content::WebContents* GetTabWebContents() const;

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // The callbacks pending the handshake to complete so the Lens suggest inputs
  // can be retrieved.
  base::OnceCallbackList<void(
      std::optional<lens::proto::LensOverlaySuggestInputs>)>
      pending_suggest_inputs_callbacks_;

  // Searchbox handler for passing in image and text selections. The handler is
  // null if the WebUI containing the searchbox has not been initialized yet,
  // like in the case of side panel opening. In addition, the handler may be
  // initialized, but the remote not yet set because the WebUI calls SetPage()
  // once it is ready to receive data from C++. Therefore, we must always check
  // that:
  //      1) searchbox_handler_ exists and
  //      2) searchbox_handler_->IsRemoteBound() is true.
  std::unique_ptr<LensSearchboxHandler> side_panel_searchbox_handler_;

  // Handler for the contextual searchbox in the overlay. The handler is
  // null if the WebUI containing the searchbox has not been initialized yet.
  // In addition, the handler may be initialized, but the remote not yet set
  // because the WebUI calls SetPage() once it is ready to receive data from
  // C++. Therefore, we must always check that:
  //      1) contextual_searchbox_handler_ exists and
  //      2) contextual_searchbox_handler_->IsRemoteBound() is true.
  // TODO(crbug.com/404941800): Does this actually need to be kept alive? Its
  // currently unused.
  std::unique_ptr<LensSearchboxHandler> overlay_searchbox_handler_;

  // Connections to the overlay ghost loader WebUI. Only valid while
  // `overlay_view_` is showing, and after the WebUI has started executing JS
  // and has bound the connection.
  mojo::Remote<lens::mojom::LensGhostLoaderPage> overlay_ghost_loader_page_;

  // Connections to the side panel ghost loader WebUI. Only valid when the side
  // panel is currently open and after the WebUI has started executing JS and
  // has bound the connection.
  mojo::Remote<lens::mojom::LensGhostLoaderPage> side_panel_ghost_loader_page_;

  // The assembly data needed for the side panel entry to be created and shown.
  std::unique_ptr<LensSearchboxInitializationData> init_data_;

  // A pending text query to be loaded in the side panel. Needed when the side
  // panel is not bound at the time of a text request.
  std::optional<std::string> pending_text_query_ = std::nullopt;

  // Must be last member.
  base::WeakPtrFactory<LensSearchboxController> weak_factory_{this};
};
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_SEARCHBOX_CONTROLLER_H_
