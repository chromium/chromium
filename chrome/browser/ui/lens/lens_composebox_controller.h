// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class LensSearchController;

namespace lens {
class LensSessionMetricsLogger;
class LensComposeboxHandler;

// Controller for the Lens compose box. This class is responsible for handling
// communications between the Lens WebUI compose box and other Lens components,
// as well as storing any state needed for the compose box. Note: This class is
// different from the LensSearchboxController, which is responsible for the old,
// non-AIM search box.
class LensComposeboxController {
 public:
  explicit LensComposeboxController(
      LensSearchController* lens_search_controller,
      Profile* profile);
  virtual ~LensComposeboxController();

  // This method is used to set up communication between this instance and the
  // compose box WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and has bound the handler.
  virtual void BindComposebox(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);

  // Issues a composebox query to the side panel results. If this is called when
  // the user is in AIM, issues a follow up query. Otherwise, issues a new AIM
  // session query.
  void IssueComposeboxQuery(const std::string& query_text);

  // Called when the focus state of the composebox changes.
  void OnFocusChanged(bool focused);

  // Cleans up any any state associated with this UI instance.
  void CloseUI();

  // Handles AIM messages from the side panel remote UI.
  void OnAimMessage(const std::vector<uint8_t>& message);

  // Resets data associated with the handshake. This allows the controller
  // to know when communication is established with AIM.
  void ResetAimHandshake();

  // Shows the Lens selection overlay. A no-op if it is already open.
  void ShowLensSelectionOverlay();

  // Adds the visual selection context to the compose box context carousel.
  void AddVisualSelectionContext(const std::string& image_data_url);

  // Clears the visual selection context.
  void ClearVisualSelectionContext();

  // Deletes the context associated with the given id.
  void DeleteContext(const base::UnguessableToken& id);

  // Clears all files.
  void ClearFiles();

  // Returns the session metrics logger for the current Lens session.
  LensSessionMetricsLogger* GetSessionMetricsLogger();

  LensComposeboxHandler* composebox_handler_for_testing() {
    return composebox_handler_.get();
  }

  const lens::proto::LensOverlaySuggestInputs&
  get_raw_suggest_inputs_for_testing() const {
    return suggest_inputs_;
  }

  lens::proto::LensOverlaySuggestInputs GetLensSuggestInputs() const;

  void UpdateSuggestInputs(
      const lens::proto::LensOverlaySuggestInputs& suggest_inputs);

  std::optional<base::UnguessableToken> vsc_image_data_id_for_testing() const {
    return vsc_image_data_ ? std::make_optional(vsc_image_data_->id)
                           : std::nullopt;
  }

 private:
  // A struct to hold the visual selection context.
  struct VisualSelectionContext {
    VisualSelectionContext(base::UnguessableToken id,
                           searchbox::mojom::SelectedFileInfoPtr file_info);
    ~VisualSelectionContext();

    VisualSelectionContext(VisualSelectionContext&&);
    VisualSelectionContext& operator=(VisualSelectionContext&&);

    base::UnguessableToken id;
    searchbox::mojom::SelectedFileInfoPtr file_info;
  };

  // Builds a SubmitQuery ClientToAimMessage message to send to the side panel
  // remote UI.
  lens::ClientToAimMessage BuildSubmitQueryMessage(
      const std::string& query_text);

  // Creates a SelectedFileInfo struct to send to the composebox for the visual
  // selection context.
  searchbox::mojom::SelectedFileInfoPtr BuildVisualSelectionFileInfo(
      const std::string& image_data_url,
      bool is_deletable);

  // Returns true if there is a pending region in the composebox, or there
  // is an active region selection in the overlay.
  bool HasRegionSelection() const;

  // Owns this.
  const raw_ptr<LensSearchController> lens_search_controller_;

  // Guarantee to outlive this.
  const raw_ptr<Profile> profile_;

  // The remote UI's capabilities. Only populated once the handshake completes.
  std::set<lens::FeatureCapability> remote_ui_capabilities_;

  // A query that was issued before the remote UI was ready. This will be sent
  // once the handshake completes.
  std::optional<std::string> pending_query_text_;

  // The class responsible for handling messages between the compose box and
  // the WebUI.
  std::unique_ptr<LensComposeboxHandler> composebox_handler_;

  // The current suggest inputs. The fields in this proto are updated
  // whenever new data is available (i.e. after an objects or interaction
  // response is received)
  lens::proto::LensOverlaySuggestInputs suggest_inputs_;

  // The current visual selection context image data URI set by the overlay if
  // any.
  std::optional<VisualSelectionContext> vsc_image_data_;
};
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_COMPOSEBOX_CONTROLLER_H_
