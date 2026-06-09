// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_AI_MODE_BUTTON_CONFIG_H_
#define CHROME_BROWSER_UI_OMNIBOX_AI_MODE_BUTTON_CONFIG_H_

#include <string>

#include "components/search_engines/search_engine_type.h"

namespace ai_mode_button_config {

// The config for an omnibox AI button.
struct AiModeButtonConfig {
  SearchEngineType id;

  // Text shown in button.
  std::u16string text;

  // Text show when hovering over button.
  std::u16string tooltip;

  // TODO(crbug.com/510389207): Wire in `navigation_url` &
  //   `navigation_url_empty`.

  // Icon shown in button.
  std::string favicon_url;

  // URL navigated when button clicked with omnibox text present.
  std::string navigation_url;

  // URL navigated when button clicked without omnibox text present.
  std::string navigation_url_empty;

  // Text announced when button is focused.
  std::u16string a11y_label;

  // Text shown in the omnibox context menu item.
  std::u16string context_menu_label;

  // Omnibox placeholder text conditionally shown when the omnibox is focused
  // and empty.
  std::u16string placeholder_text;
};

// Returns the currently selected AI button configuration.
// TODO(crbug.com/510389207): Use a observe or callback API so consumers of the
//   config are updated when the config changes.
const AiModeButtonConfig& GetCurrentAiModeButtonConfig();

}  // namespace ai_mode_button_config

#endif  // CHROME_BROWSER_UI_OMNIBOX_AI_MODE_BUTTON_CONFIG_H_
