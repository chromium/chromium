// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_client.h"

#include <optional>

#include "components/autofill/core/browser/data_model/entity_instance.h"

namespace autofill_ai {

AutofillAiClient::SavePromptAcceptanceResult::SavePromptAcceptanceResult(
    bool prompt_was_accepted,
    bool did_user_interact,
    bool did_thumbs_up_triggered,
    bool did_thumbs_down_triggered,
    std::optional<autofill::EntityInstance> entity)
    : prompt_was_accepted(prompt_was_accepted),
      did_user_interact(did_user_interact),
      did_thumbs_up_triggered(did_thumbs_up_triggered),
      did_thumbs_down_triggered(did_thumbs_down_triggered),
      entity(std::move(entity)) {}

AutofillAiClient::SavePromptAcceptanceResult::SavePromptAcceptanceResult(
    bool prompt_was_accepted)
    : prompt_was_accepted(prompt_was_accepted) {}

AutofillAiClient::SavePromptAcceptanceResult::SavePromptAcceptanceResult() =
    default;

AutofillAiClient::SavePromptAcceptanceResult::SavePromptAcceptanceResult(
    const AutofillAiClient::SavePromptAcceptanceResult&) = default;

AutofillAiClient::SavePromptAcceptanceResult::SavePromptAcceptanceResult(
    AutofillAiClient::SavePromptAcceptanceResult&&) = default;

AutofillAiClient::SavePromptAcceptanceResult&
AutofillAiClient::SavePromptAcceptanceResult::operator=(
    const AutofillAiClient::SavePromptAcceptanceResult&) = default;

AutofillAiClient::SavePromptAcceptanceResult&
AutofillAiClient::SavePromptAcceptanceResult::operator=(
    AutofillAiClient::SavePromptAcceptanceResult&&) = default;

AutofillAiClient::SavePromptAcceptanceResult::~SavePromptAcceptanceResult() =
    default;

}  // namespace autofill_ai
