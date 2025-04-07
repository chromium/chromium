// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_EXTENSION_SUGGESTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_EXTENSION_SUGGESTION_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/extension_suggestion.h"
#include "ui/gfx/image/image.h"

// A suggest result parsed from omnibox_api::SuggestResult.
struct ExtensionSuggestion {
  // An action button attached to a suggest result.
  struct Action {
    Action(std::string name,
           std::string label,
           std::string tooltip_text,
           gfx::Image icon);

    ~Action();
    Action(const Action&) = delete;
    Action& operator=(const Action&) = delete;
    Action(Action&& rhs);
    Action& operator=(Action&& rhs);

    // The string sent to the extension in the event corresponding to the user
    // clicking on the action.
    std::string name;

    // The action button label.
    std::string label;

    // The action button hover tooltip text.
    std::string tooltip_text;

    // The deserialized image data of an action icon.
    gfx::Image icon;
  };

  ExtensionSuggestion(std::string content,
                      std::string description,
                      bool deletable,
                      ACMatchClassifications match_classifications,
                      std::optional<std::vector<Action>> actions,
                      std::optional<std::string> icon_url);

  ~ExtensionSuggestion();
  ExtensionSuggestion(const ExtensionSuggestion&) = delete;
  ExtensionSuggestion& operator=(const ExtensionSuggestion&) = delete;
  ExtensionSuggestion(ExtensionSuggestion&& rhs);
  ExtensionSuggestion& operator=(ExtensionSuggestion&& rhs);

  // The text that is put into the URL bar, and that is sent to the extension
  // when the user chooses this entry.
  std::string content;

  // The text that is displayed in the URL dropdown. Can contain XML-style
  // markup for styling.
  std::string description;

  // Whether the suggest result can be deleted by the user.
  bool deletable;

  // The formatting for the suggestion text that matches the search query text.
  ACMatchClassifications match_classifications;

  // An array of actions attached to the suggestion. Only supported for
  // suggestions added in unscoped mode.
  std::optional<std::vector<Action>> actions;

  // An icon shown on the leading edge of the suggestion in the omnibox
  // dropdown. Only supported for suggestions added in unscoped mode.
  std::optional<std::string> icon_url;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_EXTENSION_SUGGESTION_H_
