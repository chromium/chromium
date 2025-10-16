// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_controller.h"

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
#include "components/tabs/public/tab_interface.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

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

lens::LensSessionMetricsLogger*
LensComposeboxController::GetSessionMetricsLogger() {
  return lens_search_controller_->lens_session_metrics_logger();
}

lens::proto::LensOverlaySuggestInputs
LensComposeboxController::GetLensSuggestInputs() const {
  if (!lens::features::GetAimSuggestionsEnabled()) {
    return lens::proto::LensOverlaySuggestInputs();
  }
  return suggest_inputs_;
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
  return client_to_aim_message;
}

}  // namespace lens
