// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/extension_suggestion.h"

ExtensionSuggestion::Action::Action(std::string name,
                                    std::string label,
                                    std::string tooltip_text,
                                    gfx::Image icon)
    : name(std::move(name)),
      label(std::move(label)),
      tooltip_text(std::move(tooltip_text)),
      icon(std::move(icon)) {}
ExtensionSuggestion::Action::~Action() = default;
ExtensionSuggestion::Action::Action(ExtensionSuggestion::Action&& rhs) =
    default;
ExtensionSuggestion::Action& ExtensionSuggestion::Action::operator=(
    ExtensionSuggestion::Action&& rhs) = default;

ExtensionSuggestion::ExtensionSuggestion(
    std::string content,
    std::string description,
    bool deletable,
    ACMatchClassifications match_classifications,
    std::optional<std::vector<Action>> actions,
    std::optional<std::string> icon_url)
    : content(std::move(content)),
      description(std::move(description)),
      deletable(deletable),
      match_classifications(std::move(match_classifications)),
      actions(std::move(actions)),
      icon_url(std::move(icon_url)) {}
ExtensionSuggestion::~ExtensionSuggestion() = default;
ExtensionSuggestion::ExtensionSuggestion(ExtensionSuggestion&& rhs) = default;
ExtensionSuggestion& ExtensionSuggestion::operator=(ExtensionSuggestion&& rhs) =
    default;
