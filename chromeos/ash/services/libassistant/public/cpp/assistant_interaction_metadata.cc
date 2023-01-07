// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/public/cpp/assistant_interaction_metadata.h"

namespace ash::assistant {

AssistantInteractionMetadata::AssistantInteractionMetadata() = default;
AssistantInteractionMetadata::AssistantInteractionMetadata(
    AssistantInteractionType type,
    AssistantQuerySource source,
    const std::string& query)
    : type(type), source(source), query(query) {}
AssistantInteractionMetadata::AssistantInteractionMetadata(
    const AssistantInteractionMetadata& suggestion) = default;
AssistantInteractionMetadata& AssistantInteractionMetadata::operator=(
    const AssistantInteractionMetadata&) = default;
AssistantInteractionMetadata::AssistantInteractionMetadata(
    AssistantInteractionMetadata&& suggestion) = default;
AssistantInteractionMetadata& AssistantInteractionMetadata::operator=(
    AssistantInteractionMetadata&&) = default;
AssistantInteractionMetadata::~AssistantInteractionMetadata() = default;

}  // namespace ash::assistant
