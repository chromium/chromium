// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/test_lens_overlay_side_panel_coordinator.h"

#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace lens {

TestLensOverlaySidePanelCoordinator::TestLensOverlaySidePanelCoordinator(
    LensSearchController* lens_search_controller)
    : LensOverlaySidePanelCoordinator(lens_search_controller) {}

TestLensOverlaySidePanelCoordinator::~TestLensOverlaySidePanelCoordinator() =
    default;

void TestLensOverlaySidePanelCoordinator::SetSidePanelIsLoadingResults(
    bool is_loading) {
  if (is_loading) {
    side_panel_loading_set_to_true_++;
    return;
  }

  side_panel_loading_set_to_false_++;
}

void TestLensOverlaySidePanelCoordinator::SendClientMessageToAim(
    const std::vector<uint8_t>& serialized_message) {
  last_sent_client_message_to_aim_.ParseFromArray(serialized_message.data(),
                                                  serialized_message.size());
  send_client_message_to_aim_call_count_++;
}

void TestLensOverlaySidePanelCoordinator::AimHandshakeReceived() {
  aim_handshake_received_call_count_++;
}

void TestLensOverlaySidePanelCoordinator::SetIsOverlayShowing(bool is_showing) {
  last_is_showing_ = is_showing;
  LensOverlaySidePanelCoordinator::SetIsOverlayShowing(is_showing);
}

void TestLensOverlaySidePanelCoordinator::ResetSidePanelTracking() {
  side_panel_loading_set_to_true_ = 0;
  side_panel_loading_set_to_false_ = 0;
  last_is_showing_.reset();
}

}  // namespace lens
