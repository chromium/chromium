// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_SETTINGS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_SETTINGS_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Global settings for the Autofill Assistant client.
//
// These settings can be updated by the server, from
// SupportsScriptResponseProto.
//
// Note that since settings can change, all classes using settings should keep a
// pointer to the single ClientSettings instance instead of making a copy.
struct ClientSettings {
  ClientSettings();
  ~ClientSettings();

  // Time between two periodic script precondition checks.
  base::TimeDelta periodic_script_check_interval =
      base::TimeDelta::FromSeconds(1);

  // Time between two element checks in the script executor.
  base::TimeDelta periodic_element_check_interval =
      base::TimeDelta::FromSeconds(1);

  // Run that many periodic checks before giving up unless something happens to
  // wake it up, such as the user touching the screen.
  int periodic_script_check_count = 10;

  // Time between two element position refreshes, when displaying highlighted
  // areas in prompt state.
  base::TimeDelta element_position_update_interval =
      base::TimeDelta::FromMilliseconds(100);

  // Maximum amount of time normal actions should implicitly wait for a selector
  // to show up.
  base::TimeDelta short_wait_for_element_deadline =
      base::TimeDelta::FromSeconds(2);

  // Time to wait between two checks of the box model, when waiting for an
  // element to become stable, such as before clicking.
  base::TimeDelta box_model_check_interval =
      base::TimeDelta::FromMilliseconds(200);

  // Maximum number of checks to run while waiting for the element position to
  // become stable.
  int box_model_check_count = 50;

  // Time to wait between two checks of the document state, when waiting for the
  // document to become ready.
  base::TimeDelta document_ready_check_interval =
      base::TimeDelta::FromMilliseconds(200);

  // Maximum number of checks to run while waiting for the document to become
  // ready.
  int document_ready_check_count = 50;

  // How much time to give users to tap undo when they tap a cancel button.
  base::TimeDelta cancel_delay = base::TimeDelta::FromSeconds(5);

  // If the user taps the overlay that many time within |tap_duration| turn the
  // UI off and give them |tap_shutdown_delay| to undo. If 0, unexpected taps
  // are ignored.
  int tap_count = 3;

  // Reset the unexpected tap counter after that time.
  base::TimeDelta tap_tracking_duration = base::TimeDelta::FromSeconds(5);

  // How much time to give users to tap undo when after |tap_count| unexpected
  // taps where
  base::TimeDelta tap_shutdown_delay = base::TimeDelta::FromSeconds(5);

  // Optional image drawn on top of overlays.
  base::Optional<OverlayImageProto> overlay_image;

  void UpdateFromProto(const ClientSettingsProto& proto);

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientSettings);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_SETTINGS_H_
