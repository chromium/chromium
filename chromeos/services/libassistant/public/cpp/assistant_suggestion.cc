// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/public/cpp/assistant_suggestion.h"

namespace chromeos {
namespace assistant {

AssistantSuggestion::AssistantSuggestion() = default;
AssistantSuggestion::AssistantSuggestion(
    base::UnguessableToken id,
    chromeos::assistant::AssistantSuggestionType type,
    const std::string& text)
    : id(id), type(type), text(text) {}
AssistantSuggestion::AssistantSuggestion(
    const AssistantSuggestion& suggestion) = default;
AssistantSuggestion& AssistantSuggestion::operator=(
    const AssistantSuggestion&) = default;
AssistantSuggestion::AssistantSuggestion(AssistantSuggestion&& suggestion) =
    default;
AssistantSuggestion& AssistantSuggestion::operator=(AssistantSuggestion&&) =
    default;
AssistantSuggestion::~AssistantSuggestion() = default;

}  // namespace assistant
}  // namespace chromeos
