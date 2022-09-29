// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"

namespace ash::assistant {

AssistantSuggestion::AssistantSuggestion() = default;
AssistantSuggestion::AssistantSuggestion(base::UnguessableToken id,
                                         AssistantSuggestionType type,
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

}  // namespace ash::assistant
