// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_autofill_manager.h"

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/actor/actor_key_metrics_recorder.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"

namespace autofill {

namespace {

std::string GetAutofillFilledTypesMetadata(
    const FormStructure& form_structure,
    const base::flat_set<FieldGlobalId>& filled_field_ids) {
  base::ListValue filled_types_list;
  for (FieldGlobalId field_id : filled_field_ids) {
    const AutofillField* field = form_structure.GetFieldById(field_id);
    if (!field) {
      continue;
    }
    std::optional<FieldType> field_type = field->autofilled_type();
    if (!field_type.has_value()) {
      continue;
    }
    filled_types_list.Append(FieldTypeToString(*field_type));
  }

  base::DictValue metadata_dict;
  metadata_dict.Set("filled_types", std::move(filled_types_list));
  std::string metadata_json;
  return base::WriteJson(metadata_dict).value_or("");
}

}  // namespace

ActorAutofillManager::ActorAutofillManager(
    AutofillClient* client,
    critical_actions::CriticalActionService* critical_action_service)
    : client_(*client),
      critical_action_service_(critical_action_service),
      key_metrics_recorder_(client) {
  managers_observation_.Observe(
      client, ScopedAutofillManagersObservation::InitializationPolicy::
                  kObservePreexistingManagers);
}

ActorAutofillManager::~ActorAutofillManager() = default;

void ActorAutofillManager::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId trigger_field_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&,
    const FillingPayload& filling_payload) {
  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }

  MaybeLogCriticalAction(manager, form_id, filled_field_ids);
}

void ActorAutofillManager::OnFillOrPreviewField(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    mojom::ActionPersistence action_persistence,
    const std::u16string& value,
    std::optional<FieldType> field_type_used) {
  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }

  MaybeLogCriticalAction(manager, form_id, {field_id});
}

bool ActorAutofillManager::IsTabInActorMode() const {
  if (base::FeatureList::IsEnabled(features::debug::kAutofillForceActorMode)) {
    return true;
  }
  return active_actor_task_.has_value();
}

void ActorAutofillManager::MaybeLogCriticalAction(
    AutofillManager& manager,
    FormGlobalId form_id,
    const base::flat_set<FieldGlobalId>& filled_field_ids) {
  if (!base::FeatureList::IsEnabled(
          critical_actions::features::kCriticalActionHistory)) {
    return;
  }

  if (!active_actor_task_) {
    return;  // Only log if the tab has an active Actor task.
  }

  if (!critical_action_service_) {
    return;
  }

  const FormStructure* const form_structure =
      manager.FindCachedFormById(form_id);
  if (!form_structure) {
    return;
  }

  critical_actions::CriticalActionEntry entry =
      critical_actions::CriticalActionEntry::Builder()
          .SetActionType(critical_actions::ActionType::kFormFill)
          .SetActionSource(critical_actions::ActionSource::kAutofill)
          .SetUrl(form_structure->source_url())
          .SetConversationId(std::move(active_actor_task_->conversation_id))
          .SetActorTaskId(
              base::NumberToString(active_actor_task_->task_id.value()))
          .SetMetadata(
              GetAutofillFilledTypesMetadata(*form_structure, filled_field_ids))
          .Build();

  // Pass the navigation ID directly to the service. If the navigation ID is 0,
  // the CriticalActionService will handle the drop and record it into the
  // histogram.
  critical_action_service_->AddCriticalActionWithNavigationId(
      entry, client_->GetNavigationId());
}

}  // namespace autofill
