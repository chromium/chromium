// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_SETTINGS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_SETTINGS_H_

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  ClientSettings(const ClientSettings&) = delete;
  ClientSettings& operator=(const ClientSettings&) = delete;

  ~ClientSettings();

  void UpdateFromProto(const ClientSettingsProto& proto);

  // Map of current display strings, if sent by the backend.
  base::flat_map<ClientSettingsProto::DisplayStringId, std::string>
      display_strings;

  // The locale of |display_strings|. If not specified, the client will display
  // strings in the current client locale. The locale follows the BCP 47 format,
  // e.g. "en-US".
  std::string display_strings_locale;

  // Time between two periodic script precondition checks.
  base::TimeDelta periodic_script_check_interval = base::Seconds(1);

  // Time between two element checks in the script executor.
  base::TimeDelta periodic_element_check_interval = base::Seconds(1);

  // Run that many periodic checks before giving up unless something happens to
  // wake it up, such as the user touching the screen.
  int periodic_script_check_count = 10;

  // Time between two element position refreshes, when displaying highlighted
  // areas in prompt state.
  base::TimeDelta element_position_update_interval = base::Milliseconds(100);

  // Maximum amount of time normal actions should implicitly wait for a selector
  // to show up.
  base::TimeDelta short_wait_for_element_deadline = base::Seconds(2);

  // Time to wait between two checks of the box model, when waiting for an
  // element to become stable, such as before clicking.
  base::TimeDelta box_model_check_interval = base::Milliseconds(200);

  // Maximum number of checks to run while waiting for the element position to
  // become stable.
  int box_model_check_count = 50;

  // Time to wait while checking the document state, when waiting for the
  // document to become ready.
  base::TimeDelta document_ready_check_timeout = base::Seconds(10);

  // How much time to give users to tap undo when they tap a cancel button.
  base::TimeDelta cancel_delay = base::Seconds(5);

  // If the user taps the overlay that many time within |tap_duration| turn the
  // UI off and give them |tap_shutdown_delay| to undo. If 0, unexpected taps
  // are ignored.
  int tap_count = 3;

  // Reset the unexpected tap counter after that time.
  base::TimeDelta tap_tracking_duration = base::Seconds(5);

  // How much time to give users to tap undo when after |tap_count| unexpected
  // taps where
  base::TimeDelta tap_shutdown_delay = base::Seconds(5);

  // Optional image drawn on top of overlays.
  absl::optional<OverlayImageProto> overlay_image;

  // Optional settings intended for integration tests.
  absl::optional<ClientSettingsProto::IntegrationTestSettings>
      integration_test_settings;

  float talkback_sheet_size_fraction = 0.5f;

  // Optional settings to enable back button error in BottomSheet instead of
  // Snackbar.
  absl::optional<ClientSettingsProto::BackButtonSettings> back_button_settings;

  // Whether to show warnings related to a slow connection to the user.
  bool enable_slow_connection_warnings = false;

  // Whether to show warnings related to a slow website to the user.
  bool enable_slow_website_warnings = false;

  // If true, only one warning will be shown to the user, i.e. either the slow
  // connection or website, depending on which one triggers first.
  bool only_show_warning_once = true;

  // If true, the slow connection warning will be shown only once.
  bool only_show_connection_warning_once = true;

  // If true, the website warning will be shown only once.
  bool only_show_website_warning_once = true;

  // Defines the maximum wait on a dom find element operation before showing
  // the slow website warning.
  base::TimeDelta warning_delay = base::Milliseconds(1500);

  // Defines the number of consecutive slow roundtrips allowed before showing
  // the slow connection warning.
  int max_consecutive_slow_roundtrips = 3;

  // Defines the threshold above which a roundtrip is considered too slow.
  base::TimeDelta slow_roundtrip_threshold = base::Milliseconds(1500);

  // The message to show as a warning to inform the user of a slow connection.
  // If this is not set, no warning will be shown in case of slow connection.
  std::string slow_connection_message = "";

  // The message to show as a warning to inform the user of a slow website.
  // If this is not set, no warning will be shown in case of a slow website.
  std::string slow_website_message = "";

  // The minimum duration that the message will be shown for (only applies to
  // the slow connection messages).
  base::TimeDelta minimum_warning_duration = base::Milliseconds(1500);

  // Whether the warning message should replace the current status message or
  // should be concatenated.
  ClientSettingsProto::SlowWarningSettings::MessageMode message_mode =
      ClientSettingsProto::SlowWarningSettings::REPLACE;

  // Extra time SelectorObserver has to finish. If it takes longer than
  // max_wait_time + extra_timeout (this value) it assumes something went
  // wrong and fails with a |TIMED_OUT| error. SelectorObserver only counts
  // time spent waiting so a extra delay of 1 to 10 seconds for javascript
  // execution and checking selectors is conceivable.
  base::TimeDelta selector_observer_extra_timeout = base::Seconds(15);

  // Wait until no DOM changes are received for this amount of time to check
  // the selectors. An interval of 0 effectively disables debouncing.
  base::TimeDelta selector_observer_debounce_interval = base::Milliseconds(100);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_CLIENT_SETTINGS_H_
