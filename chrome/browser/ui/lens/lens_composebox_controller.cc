// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_controller.h"

#include "chrome/browser/ui/lens/lens_composebox_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

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
  // TODO(crbug.com/435504019): Implement filling out all details of the query
  // proto.
  lens::ClientToAimMessage client_to_aim_message;
  lens::SubmitQuery* submit_query_message =
      client_to_aim_message.mutable_submit_query();
  submit_query_message->mutable_payload()->set_query_text(query_text);
  submit_query_message->mutable_payload()->set_query_text_source(
      lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT);

  // Convert Proto to bytes to send over the API channel.
  const size_t size = client_to_aim_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  client_to_aim_message.SerializeToArray(&serialized_message[0], size);

  // Send the message to the remote UI.
  lens_search_controller_->lens_overlay_side_panel_coordinator()
      ->SendClientMessageToAim(serialized_message);
}

void LensComposeboxController::CloseUI() {
  composebox_handler_.reset();
}

}  // namespace lens
