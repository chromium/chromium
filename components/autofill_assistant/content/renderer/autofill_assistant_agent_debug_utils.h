// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_DEBUG_UTILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_DEBUG_UTILS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "components/autofill_assistant/content/renderer/autofill_assistant_model_executor_result.h"
#include "third_party/blink/public/web/modules/autofill_assistant/node_signals.h"

namespace autofill_assistant {

using SemanticPredictionLabelMap = base::flat_map<int, std::string>;
using SemanticLabelsPair =
    std::pair<SemanticPredictionLabelMap, SemanticPredictionLabelMap>;

std::string NodeSignalsToDebugString(
    const blink::AutofillAssistantNodeSignals& node_signals);

// Base64 encoded string that contains a JSON object in the following format:
// {
//    "roles":      [{"id": role_id,      "name": label}...],
//    "objectives": [{"id": objective_id, "name": label}...]
// }
// This function decodes and parses the string and returns a
// map<("roles"|"objectives"), map<id, label>> object if it's a valid JSON.
SemanticLabelsPair DecodeSemanticPredictionLabelsJson(std::string encodedJson);

// Maps the role and objective indexes from a semantic prediction to their
// corresponding labels.
std::u16string SemanticPredictionResultToDebugString(
    SemanticPredictionLabelMap roles,
    SemanticPredictionLabelMap objectives,
    const ModelExecutorResult& result,
    bool ignore_objective);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_CONTENT_RENDERER_AUTOFILL_ASSISTANT_AGENT_DEBUG_UTILS_H_
