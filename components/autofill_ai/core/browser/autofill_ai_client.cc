// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/autofill_ai_client.h"

#include <optional>
#include <utility>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill_ai {

AutofillAiClient::SaveOrUpdatePromptResult::SaveOrUpdatePromptResult(
    bool did_user_decline,
    std::optional<autofill::EntityInstance> entity)
    : did_user_decline(did_user_decline), entity(std::move(entity)) {}

AutofillAiClient::SaveOrUpdatePromptResult::SaveOrUpdatePromptResult() =
    default;

AutofillAiClient::SaveOrUpdatePromptResult::SaveOrUpdatePromptResult(
    const AutofillAiClient::SaveOrUpdatePromptResult&) = default;

AutofillAiClient::SaveOrUpdatePromptResult::SaveOrUpdatePromptResult(
    AutofillAiClient::SaveOrUpdatePromptResult&&) = default;

AutofillAiClient::SaveOrUpdatePromptResult&
AutofillAiClient::SaveOrUpdatePromptResult::operator=(
    const AutofillAiClient::SaveOrUpdatePromptResult&) = default;

AutofillAiClient::SaveOrUpdatePromptResult&
AutofillAiClient::SaveOrUpdatePromptResult::operator=(
    AutofillAiClient::SaveOrUpdatePromptResult&&) = default;

AutofillAiClient::SaveOrUpdatePromptResult::~SaveOrUpdatePromptResult() =
    default;

}  // namespace autofill_ai
