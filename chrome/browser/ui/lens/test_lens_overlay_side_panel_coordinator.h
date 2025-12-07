// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_

#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace lens {

// Helper for testing features that use the LensOverlaySidePanelCoordinator.
// The only logic in this class should be for tracking sent request data. Actual
// LensOverlaySidePanelCoordinator logic should not be stubbed out.
class TestLensOverlaySidePanelCoordinator
    : public LensOverlaySidePanelCoordinator {
 public:
  explicit TestLensOverlaySidePanelCoordinator(
      LensSearchController* lens_search_controller);
  ~TestLensOverlaySidePanelCoordinator() override;

  void SetSidePanelIsLoadingResults(bool is_loading) override;

  void SendClientMessageToAim(
      const std::vector<uint8_t>& serialized_message) override;

  void AimHandshakeReceived() override;

  void SetIsOverlayShowing(bool is_showing) override;

  void ResetSidePanelTracking();

  lens::ClientToAimMessage last_sent_client_message_to_aim_;
  int send_client_message_to_aim_call_count_ = 0;
  int aim_handshake_received_call_count_ = 0;
  int side_panel_loading_set_to_true_ = 0;
  int side_panel_loading_set_to_false_ = 0;
  std::optional<bool> last_is_showing_;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_TEST_LENS_OVERLAY_SIDE_PANEL_COORDINATOR_H_
