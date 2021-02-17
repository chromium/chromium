// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/assistant_service.h"

namespace chromeos {
namespace assistant {

namespace {

AssistantService* g_instance = nullptr;

}  // namespace

// static
AssistantService* AssistantService::Get() {
  return g_instance;
}

AssistantService::AssistantService() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AssistantService::~AssistantService() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

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

bool AssistantInteractionSubscriber::OnOpenAppResponse(
    const AndroidAppInfo& app_info) {
  return false;
}

}  // namespace assistant
}  // namespace chromeos
