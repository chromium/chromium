// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_AUTOFILL_AI_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_AUTOFILL_AI_METRICS_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

class EntityType;

// Logs metrics related to the user seeing an IPH, accepting it and eventually
// seeing or accepting the FFR dialog.
enum class AutofillAiOptInFunnelEvents {
  kIphShown = 0,
  kFFRDialogShown = 1,
  kFFRLearnMoreButtonClicked = 2,
  kFFRDialogAccepted = 3,
  kMaxValue = kFFRDialogAccepted,
};

void LogOptInFunnelEvent(AutofillAiOptInFunnelEvents event);

void LogLocalEntitiesDeduplicationMetrics(
    const base::flat_map<EntityType, size_t>&
        local_entities_considered_for_deduplication_per_type,
    const base::flat_map<EntityType, size_t>&
        local_entities_deduplicated_per_type);

void LogStoredEntitiesCount(base::span<const EntityInstance> entities);

std::string_view EntityTypeToMetricsString(EntityType type);

std::string_view EntityRecordTypeToMetricsString(
    EntityInstance::RecordType record_type);

std::string_view EntityPromptTypeToMetricsString(
    AutofillClient::AutofillAiImportPromptType prompt_type);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_AUTOFILL_AI_METRICS_H_
