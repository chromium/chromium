// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_

#include <string>
#include <vector>

#include "base/time/time.h"

namespace compose {

// How Compose should position its dialog if there isn't enough space above or
// below the underlying form field (the `anchor`) on the screen.
enum class DialogFallbackPositioningStrategy : int {
  // This adjusts the position so that the dialog's top border will never have
  // to move to keep the bottom border onscreen, regardless of the actual size
  // of the dialog. This may result in the dialog being rendered higher on
  // screen that expected, obscuring the underlying element more than absolutely
  // necessary. It has the advantage that the dialog is not repositioned when it
  // grows, resulting in less jarring resizes.
  kShiftUpUntilMaxSizeIsOnscreen = 0,

  // This adjusts the dialog to be centered over its underlying form element,
  // which has the advantage of always being close to the relevant page content,
  // but will obscure more of the form field than other strategies.
  kCenterOnAnchorRect = 1,

  // This adjusts the dialog to be onscreen, but no further. This has the
  // advantage of not obscuring the underlying element more than necesasary, but
  // the downside that the dialog will move in position as it resizes. In
  // practice this is more visually jarring than just making the dialog bigger.
  kShiftUpUntilOnscreen = 2
};

// The Compose configuration. Default values appear below. Always use
// |GetComposeConfig()| to get the current configuration.
struct Config {
  // The minimum number of words needed for a valid user input.
  unsigned int input_min_words = 3;
  // The maximum number of words allowed for a valid user input.
  unsigned int input_max_words = 500;
  // The maximum number of characters allowed for a valid user input.
  unsigned int input_max_chars = 2500;
  // The maximum number of bytes allowed in the inner text.
  unsigned int inner_text_max_bytes = 1024 * 1024;
  // The maximum number of bytes allowed in the inner text.
  unsigned int trimmed_inner_text_max_chars = 12000;
  // The maximum number of bytes allowed in the inner text.
  unsigned int trimmed_inner_text_header_length = 4000;

  // The maximum number of AX nodes to gather for page context.
  unsigned int max_ax_node_count_for_page_context = 5000;

  // Whether to send a compose when the dialog is first opened,
  // if there is an acceptable input text selected.
  bool auto_submit_with_selection = false;

  // Whether to enable the nudge on focus when there is saved state.
  bool saved_state_nudge_enabled = true;

  // Whether to enable the proactive nudge with no saved state.
  bool proactive_nudge_enabled = true;

  // Use the compact UI for proactive nudge.
  bool proactive_nudge_compact_ui = true;

  // Whether or not the proactive nudge is shown at the cursor.
  bool is_nudge_shown_at_cursor = true;

  // Used to randomly hide the nudge in order to reduce exposure, experimental
  // flag for triggering research experiments only. If param is greater than
  // `1`, always shows. If param is negative, never shows.
  double proactive_nudge_show_probability = 0.02;

  // When segmentation is enabled and working, this parameter controls how often
  // we randomly decide to show the proactive nudge regardless of the
  // segmentation platform's response. Nudges shown in this way contribute to
  // training data for the segmentation platform.
  double proactive_nudge_force_show_probability = 0.004;

  // Whether to collect training data for the segmentation platform any time the
  // nudge is shown. If false, training data is only collected when the nudge is
  // randomly force-shown, see `proactive_nudge_force_show_probability`.
  bool proactive_nudge_always_collect_training_data = false;

  // Ignores OptGuide decision to disable the nudge. Does not bypass other
  // hint decisions.
  bool proactive_nudge_bypass_optimization_guide = false;

  // Uses segmentation platform to predict nudge utility.
  bool proactive_nudge_segmentation = true;

  // How long to wait to show the proactive nudge after focus. A non-positive
  // value disables the focus nudge.
  base::TimeDelta proactive_nudge_focus_delay = base::Seconds(0);
  // How long to wait to show the proactive nudge after valid text input. A
  // non-positive value disables the after input nudge.
  base::TimeDelta proactive_nudge_text_settled_delay = base::Seconds(1);

  // How many text change events to wait for before showing the after text input
  // nudge.
  int proactive_nudge_text_change_count = 10;

  // Whether the nudge on selection should be enabled.
  bool selection_nudge_enabled = false;

  // Minimum selection length needed before showing the selection nudge.
  unsigned int selection_nudge_length = 30;

  // How long to wait to show the selection nudge.
  base::TimeDelta selection_nudge_delay = base::Milliseconds(500);

  // Whether the selection nudge can be shown more than once per focus.
  bool selection_nudge_once_per_focus = false;

  // If true, nudge at most once per field per navigation. If false, at most
  // once per field per focus.
  bool proactive_nudge_field_per_navigation = true;

  unsigned int nudge_field_change_event_max = 3;

  // The duration that the saved state notification is shown before
  // auto-dismissal.
  unsigned int saved_state_timeout_milliseconds = 2000;

  // The delay to wait to show the saved state notification after the compose
  // dialog loses focus.
  unsigned int focus_lost_delay_milliseconds = 100;

  // Whether the dialog should prioritize staying within bounds of the browser
  // window above visibility of the anchor rect.
  bool stay_in_window_bounds = true;

  // The dialog positioning strategy to use if there isn't enough space above or
  // below the anchor element.
  DialogFallbackPositioningStrategy positioning_strategy =
      DialogFallbackPositioningStrategy::kShiftUpUntilMaxSizeIsOnscreen;

  // The threshold for Compose request latency before showing a client-side
  // error message.
  unsigned int request_latency_timeout_seconds = 20;

  // Finch-controllable list of countries where Compose should be enabled. The
  // default value contains countries where it was already fully launched.
  std::vector<std::string> enabled_countries = {
      "bd", "ca", "gh", "in", "ke", "my", "ng", "ph",
      "pk", "sg", "tz", "ug", "us", "zm", "zw",
  };

  // The threshold for the lifetime of a Compose session. A session whose
  // lifetime has exceeded this threshold will be replaced with a new session
  // the next time there is an attempt to resume it.
  base::TimeDelta session_max_allowed_lifetime = base::Minutes(30);

  Config();
  Config(const Config& other);
  ~Config();
};

// Gets the current configuration.
const Config& GetComposeConfig();

Config& GetMutableConfigForTesting();
void ResetConfigForTesting();

}  // namespace compose

#endif  // COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_
