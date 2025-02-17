// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_client.h"

#include <optional>
#include <utility>

#include "components/autofill/core/browser/data_model/entity_instance.h"

namespace autofill_ai {

AutofillAiClient::SavePromptAcceptanceResult::SavePromptAcceptanceResult(
    bool did_user_interact,
    std::optional<autofill::EntityInstance> entity)
    : did_user_interact(did_user_interact), entity(std::move(entity)) {}

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
