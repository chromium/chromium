// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_

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
  // Whether to send a compose when the dialog is first opened,
  // if there is an acceptable input text selected.
  bool auto_submit_with_selection = false;
  // If nudging is enabled, show the popup when focus appears on a field with no
  // saved state.
  bool popup_with_no_saved_state = false;
  // If nudging is enabled, show the popup when focus appears on a field with
  // saved state.
  bool popup_with_saved_state = true;

  // The duration that the saved state notification is shown before
  // auto-dismissal.
  unsigned int saved_state_timeout_milliseconds = 2000;

  // Whether the dialog should prioritize staying within bounds of the browser
  // window above visibility of the anchor rect.
  bool stay_in_window_bounds = true;

  // The dialog positioning strategy to use if there isn't enough space above or
  // below the anchor element.
  DialogFallbackPositioningStrategy positioning_strategy =
      DialogFallbackPositioningStrategy::kShiftUpUntilMaxSizeIsOnscreen;

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
