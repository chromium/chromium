// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_controller.h"

#include "base/base64url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_composebox_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_payload_construction.h"
#include "components/lens/lens_url_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"

namespace {
lens::LensOverlayVisualInputType LensMimeTypeToVisualInputType(
    lens::MimeType mime_type) {
  switch (mime_type) {
    case lens::MimeType::kPdf:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_PDF;
    case lens::MimeType::kAnnotatedPageContent:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_WEBPAGE;
    default:
      return lens::LensOverlayVisualInputType::VISUAL_INPUT_TYPE_UNKNOWN;
  }
}
}  // namespace

namespace lens {

LensComposeboxController::VisualSelectionContext::VisualSelectionContext(
    base::UnguessableToken id,
    searchbox::mojom::SelectedFileInfoPtr file_info)
    : id(id), file_info(std::move(file_info)) {}
LensComposeboxController::VisualSelectionContext::~VisualSelectionContext() =
    default;
LensComposeboxController::VisualSelectionContext::VisualSelectionContext(
    VisualSelectionContext&&) = default;
LensComposeboxController::VisualSelectionContext&
LensComposeboxController::VisualSelectionContext::operator=(
    VisualSelectionContext&&) = default;

LensComposeboxController::LensComposeboxController(
    LensSearchController* lens_search_controller,
    Profile* profile)
    : lens_search_controller_(lens_search_controller), profile_(profile) {}

LensComposeboxController::~LensComposeboxController() = default;

void LensComposeboxController::BindComposebox(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  composebox_handler_.reset();
  composebox_handler_ = std::make_unique<LensComposeboxHandler>(
      this, profile_, lens_search_controller_->GetTabInterface()->GetContents(),
      std::move(pending_handler), std::move(pending_page),
      std::move(pending_searchbox_handler));

  // TODO(crbug.com/435288212): Move searchbox mojom to use factory pattern.
  composebox_handler_->SetPage(std::move(pending_searchbox_page));

  // Set the visual selection context if it was already made before the
  // composebox was bound.
  if (vsc_image_data_.has_value()) {
    composebox_handler_->AddFileContextFromBrowser(
        vsc_image_data_->id, vsc_image_data_->file_info.Clone());
  }

  // Record that the composebox was shown. The composebox handler is always
  // bound, so check if the composebox is actually enabled before logging as
  // shown.
  if (lens::IsAimM3Enabled(profile_) &&
      lens::features::GetAimSearchboxEnabled()) {
    GetSessionMetricsLogger()->OnAimComposeboxShown();
  }
}

void LensComposeboxController::IssueComposeboxQuery(
    const std::string& query_text) {
  if (!lens::IsAimM3Enabled(profile_)) {
    return;
  }

  // Record that a query was submitted. This should be first in this method to
  // ensure it is recorded even if the query is queued to be issued later.
  GetSessionMetricsLogger()->OnAimQuerySubmitted();

  // Can only issue a query if the remote UI supports the DEFAULT feature.
  if (remote_ui_capabilities_.empty() ||
      !remote_ui_capabilities_.contains(lens::FeatureCapability::DEFAULT)) {
    // Store the query and issue it again once the handshake completes.
    pending_query_text_ = query_text;
    return;
  }

  // TODO(crbug.com/436318377): Reupload page content if needed.
  lens::ClientToAimMessage submit_query_message =
      BuildSubmitQueryMessage(query_text);

  // Convert Proto to bytes to send over the API channel.
  const size_t size = submit_query_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  submit_query_message.SerializeToArray(&serialized_message[0], size);

  // Send the message to the remote UI.
  lens_search_controller_->lens_overlay_side_panel_coordinator()
      ->SendClientMessageToAim(serialized_message);

  // Focus the iframe in the side panel. This moves screen reader focus to the
  // results frame so the loading of AIM results are properly announced.
  lens_search_controller_->lens_overlay_side_panel_coordinator()
      ->FocusResultsFrame();

  // Record that a query was issued.
  GetSessionMetricsLogger()->OnAimQueryIssued();

  // When issuing a composebox query, the overlay should always be dismissed.
  // This is a no-op if the overlay is already closed.
  lens_search_controller_->HideOverlay();
}

void LensComposeboxController::OnFocusChanged(bool focused) {
  // Ignore if the user left focus.
  if (!focused) {
    return;
  }

  // Record that the composebox was focused.
  GetSessionMetricsLogger()->OnAimComposeboxFocused();

  // Ignore if recontextualization on focus is disabled.
  if (!lens::features::GetShouldComposeboxContextualizeOnFocus()) {
    return;
  }

  // If the composebox becomes focused, the user is showing intent to issue a
  // new query. Upload the new page content for contextualization. The content
  // is updated asynchronously, but this class does not need to wait for the
  // update to complete, so a callback is not needed.
  lens_search_controller_->lens_search_contextualization_controller()
      ->TryUpdatePageContextualization(base::DoNothing());
}

void LensComposeboxController::CloseUI() {
  ResetAimHandshake();
  pending_query_text_.reset();
  composebox_handler_.reset();
  suggest_inputs_.Clear();
  vsc_image_data_.reset();
}

void LensComposeboxController::OnAimMessage(
    const std::vector<uint8_t>& message) {
  // Ignore the message if the searchbox is disabled.
  if (!lens::IsAimM3Enabled(profile_)) {
    return;
  }
  // Try and parse the message as an AimToClientMessage. Since it is the only
  // message type we expect, if parsing fails, we can assume it is a malformed
  // message and ignore it.
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    remote_ui_capabilities_.clear();
    // Store the remote UI's capabilities. This should only be done once.
    for (int capability_int :
         aim_to_client_message.handshake_response().capabilities()) {
      remote_ui_capabilities_.insert(
          static_cast<lens::FeatureCapability>(capability_int));
    }

    lens_search_controller_->lens_overlay_side_panel_coordinator()
        ->AimHandshakeReceived();
    GetSessionMetricsLogger()->OnAimHandshakeCompleted();

    // If there was a pending query, issue it now that the handshake is
    // complete.
    if (pending_query_text_.has_value()) {
      IssueComposeboxQuery(pending_query_text_.value());
      pending_query_text_.reset();
    }
  }
}

void LensComposeboxController::ResetAimHandshake() {
  remote_ui_capabilities_.clear();
}

void LensComposeboxController::ShowLensSelectionOverlay() {
  lens_search_controller_->OpenLensOverlayInCurrentSession();
}

void LensComposeboxController::AddVisualSelectionContext(
    const std::string& image_data_url) {
  if (!lens::features::GetEnableLensButtonInSearchbox()) {
    return;
  }

  // Clear any existing visual selection context.
  ClearVisualSelectionContext();

  vsc_image_data_.emplace(
      base::UnguessableToken::Create(),
      BuildVisualSelectionFileInfo(image_data_url, /*is_deletable=*/true));
  // If the composebox handler is not yet bound, the image will be added when
  // the composebox is bound.
  if (composebox_handler_) {
    composebox_handler_->AddFileContextFromBrowser(vsc_image_data_->id,
                                        vsc_image_data_->file_info.Clone());
  }
}

void LensComposeboxController::ClearVisualSelectionContext() {
  // If there is existing visual selection context, mark it as expired. There
  // should only be one visual selection context at a time. The UI should
  // appropriately remove the existing thumbnail.
  if (vsc_image_data_ && composebox_handler_) {
    composebox_handler_->OnContextualInputStatusChanged(
        vsc_image_data_->id,
        composebox_query::mojom::FileUploadStatus::kUploadExpired,
        std::nullopt);
  }
  vsc_image_data_.reset();
}

void LensComposeboxController::DeleteContext(const base::UnguessableToken& id) {
  // If the id matches the visual selection context, delete it and notify
  // the overlay to clear the visual selection.
  if (vsc_image_data_ && vsc_image_data_->id == id) {
    vsc_image_data_.reset();
    lens_search_controller_->lens_overlay_controller()->ClearAllSelections();
  }
}

void LensComposeboxController::ClearFiles() {
  ClearVisualSelectionContext();
  lens_search_controller_->lens_overlay_controller()->ClearAllSelections();
}

lens::LensSessionMetricsLogger*
LensComposeboxController::GetSessionMetricsLogger() {
  return lens_search_controller_->lens_session_metrics_logger();
}

lens::proto::LensOverlaySuggestInputs
LensComposeboxController::GetLensSuggestInputs() const {
  if (!lens::features::GetAimSuggestionsEnabled()) {
    return lens::proto::LensOverlaySuggestInputs();
  }
  lens::proto::LensOverlaySuggestInputs suggest_inputs = suggest_inputs_;
  // If the overlay is closed and there is not a region selection in the
  // composebox, clear the vsint param so that the server will not focus
  // suggestions on the stale region.
  if (!HasRegionSelection() &&
      lens::features::ClearVsintWhenNoRegionSelection()) {
    suggest_inputs.clear_encoded_visual_search_interaction_log_data();
  }

  if (lens::features::GetLensAimSuggestionsType() ==
          lens::features::LensAimSuggestionsType::kMultimodal &&
      lens::features::GetLensOverlaySendVitAsImageForLensSuggest()) {
    suggest_inputs.set_contextual_visual_input_type(
        kImageVisualInputTypeQueryParameterValue);
  }
  return suggest_inputs;
}

void LensComposeboxController::UpdateSuggestInputs(
    const lens::proto::LensOverlaySuggestInputs& suggest_inputs) {
  suggest_inputs_ = suggest_inputs;
}

lens::ClientToAimMessage LensComposeboxController::BuildSubmitQueryMessage(
    const std::string& query_text) {
  lens::ClientToAimMessage client_to_aim_message;
  lens::SubmitQuery* submit_query_message =
      client_to_aim_message.mutable_submit_query();

  // Set the query text and source.
  submit_query_message->mutable_payload()->set_query_text(query_text);
  submit_query_message->mutable_payload()->set_query_text_source(
      lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT);

  // Populate the Lens related data from the active query flow.
  lens::LensImageQueryData* lens_image_query_data =
      submit_query_message->mutable_payload()->add_lens_image_query_data();
  LensOverlayQueryController* query_controller =
      lens_search_controller_->lens_overlay_query_controller();
  LensSearchContextualizationController* contextualization_controller =
      lens_search_controller_->lens_search_contextualization_controller();
  LensOverlayController* overlay_controller =
      lens_search_controller_->lens_overlay_controller();
  lens_image_query_data->set_search_session_id(
      query_controller->search_session_id());

  const auto& primary_content_type =
      contextualization_controller->primary_content_type();
  const auto media_type =
      overlay_controller->HasRegionSelection()
          ? lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE
          : MimeTypeToMediaType(primary_content_type,
                                /*has_viewport_screenshot=*/true);
  lens_image_query_data->mutable_request_id()->CopyFrom(
      *query_controller->GetNextRequestId(lens::RequestIdUpdateMode::kSearchUrl,
                                          media_type));
  lens_image_query_data->set_visual_input_type(
      LensMimeTypeToVisualInputType(primary_content_type));

  // Add the latest visual search interaction data to the query if it exists.
  std::optional<lens::LensOverlayVisualSearchInteractionData>
      visual_search_interaction_data =
          query_controller->GetVisualSearchInteractionData();
  if (visual_search_interaction_data &&
      overlay_controller->HasRegionSelection()) {
    lens_image_query_data->mutable_visual_search_interaction_data()->CopyFrom(
        visual_search_interaction_data.value());
  } else {
    lens_image_query_data->mutable_visual_search_interaction_data()->CopyFrom(
        lens::LensOverlayVisualSearchInteractionData());
  }
  return client_to_aim_message;
}

searchbox::mojom::SelectedFileInfoPtr
LensComposeboxController::BuildVisualSelectionFileInfo(
    const std::string& image_data_url,
    bool is_deletable) {
  searchbox::mojom::SelectedFileInfoPtr file_info =
      searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "Visual Selection";
  file_info->mime_type = "image/png";
  file_info->image_data_url = image_data_url;
  file_info->is_deletable = is_deletable;
  return file_info;
}

bool LensComposeboxController::HasRegionSelection() const {
  const bool has_region_in_composebox = vsc_image_data_.has_value();
  const bool has_region_in_overlay =
      lens_search_controller_->lens_overlay_controller()->HasRegionSelection();
  return has_region_in_overlay || has_region_in_composebox;
}
}  // namespace lens
