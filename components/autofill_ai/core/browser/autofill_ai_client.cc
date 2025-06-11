// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_client.h"

#include <optional>
#include <utility>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill_ai {

AutofillAiClient::EntitySaveOrUpdatePromptResult::
    EntitySaveOrUpdatePromptResult(
        bool did_user_decline,
        std::optional<autofill::EntityInstance> entity)
    : did_user_decline(did_user_decline), entity(std::move(entity)) {}

AutofillAiClient::EntitySaveOrUpdatePromptResult::
    EntitySaveOrUpdatePromptResult() = default;

AutofillAiClient::EntitySaveOrUpdatePromptResult::
    EntitySaveOrUpdatePromptResult(
        const AutofillAiClient::EntitySaveOrUpdatePromptResult&) = default;

AutofillAiClient::EntitySaveOrUpdatePromptResult::
    EntitySaveOrUpdatePromptResult(
        AutofillAiClient::EntitySaveOrUpdatePromptResult&&) = default;

AutofillAiClient::EntitySaveOrUpdatePromptResult&
AutofillAiClient::EntitySaveOrUpdatePromptResult::operator=(
    const AutofillAiClient::EntitySaveOrUpdatePromptResult&) = default;

AutofillAiClient::EntitySaveOrUpdatePromptResult&
AutofillAiClient::EntitySaveOrUpdatePromptResult::operator=(
    AutofillAiClient::EntitySaveOrUpdatePromptResult&&) = default;

AutofillAiClient::EntitySaveOrUpdatePromptResult::
    ~EntitySaveOrUpdatePromptResult() = default;

}  // namespace autofill_ai
