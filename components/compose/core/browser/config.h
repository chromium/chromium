// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_
#define COMPONENTS_COMPOSE_CORE_BROWSER_CONFIG_H_

namespace compose {

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
  bool auto_submit_with_selection = true;
  // If nudging is enabled, show the popup when focus appears on a field with no
  // saved state.
  bool popup_with_no_saved_state = false;
  // If nudging is enabled, show the popup when focus appears on a field with
  // saved state.
  bool popup_with_saved_state = true;

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
