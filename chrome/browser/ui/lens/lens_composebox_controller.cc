// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_controller.h"

#include "chrome/browser/ui/lens/lens_composebox_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
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
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}

LensComposeboxController::~LensComposeboxController() = default;

void LensComposeboxController::BindComposebox(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  // The composebox handler should only be bound once.
  CHECK(composebox_handler_ == nullptr);
  composebox_handler_ = std::make_unique<LensComposeboxHandler>(
      this, std::move(pending_handler), std::move(pending_page),
      std::move(pending_searchbox_handler));
}

void LensComposeboxController::IssueComposeboxQuery(
    const std::string& query_text) {
  if (!lens::features::GetAimSearchboxEnabled()) {
    return;
  }
  // Can only issue a query if the remote UI supports the DEFAULT feature.
  if (remote_ui_capabilities_.empty() ||
      !remote_ui_capabilities_.contains(lens::FeatureCapability::DEFAULT)) {
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
}

void LensComposeboxController::CloseUI() {
  composebox_handler_.reset();
}

void LensComposeboxController::OnAimMessage(
    const std::vector<uint8_t>& message) {
  // Try and parse the message as an AimToClientMessage. Since it is the only
  // message type we expect, if parsing fails, we can assume it is a malformed
  // message and ignore it.
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    // Store the remote UI's capabilities. This should only be done once.
    for (int capability_int :
         aim_to_client_message.handshake_response().capabilities()) {
      remote_ui_capabilities_.insert(
          static_cast<lens::FeatureCapability>(capability_int));
    }

    lens_search_controller_->lens_overlay_side_panel_coordinator()
        ->AimHandshakeReceived();
  }
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
  lens_image_query_data->set_search_session_id(
      query_controller->search_session_id());
  lens_image_query_data->mutable_request_id()->CopyFrom(
      *query_controller->GetNextRequestId(
          lens::RequestIdUpdateMode::kSearchUrl));
  lens_image_query_data->set_visual_input_type(LensMimeTypeToVisualInputType(
      contextualization_controller->primary_content_type()));
  return client_to_aim_message;
}

}  // namespace lens
