// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/actor/actor_autofill_manager.h"

#include <optional>
#include <utility>
#include <variant>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/actor/actor_critical_action_util.h"
#include "components/autofill/core/browser/actor/actor_key_metrics_recorder.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "components/critical_actions/core/browser/features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// Returns the contact info attributes of the fields filled from a profile, or
// `std::nullopt` if the form isn't cached.
std::optional<critical_actions::FormFillTypes> GetFilledContactInfoTypes(
    const AutofillManager& manager,
    FormGlobalId form_id,
    const base::flat_set<FieldGlobalId>& filled_field_ids) {
  const FormStructure* form_structure = manager.FindCachedFormById(form_id);
  if (!form_structure) {
    return std::nullopt;
  }
  critical_actions::FormFillTypes types;
  for (FieldGlobalId field_id : filled_field_ids) {
    const AutofillField* field = form_structure->GetFieldById(field_id);
    if (!field || !field->autofilled_type()) {
      continue;
    }
    std::optional<critical_actions::FormFillType> type =
        ToFormFillType(GroupTypeOfFieldType(*field->autofilled_type()));
    if (type && critical_actions::GetFormFillGroup(*type) ==
                    critical_actions::ActionType::kFormFillContactInfo) {
      types.Put(*type);
    }
  }
  return types;
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
  // Fills that didn't fill any field aren't logged.
  if (action_persistence != mojom::ActionPersistence::kFill ||
      filled_field_ids.empty()) {
    return;
  }

  // The group is chosen from the payload, not from each field's type.
  std::visit(
      absl::Overload{
          [&](const AutofillProfile*) {
            if (std::optional<critical_actions::FormFillTypes> types =
                    GetFilledContactInfoTypes(manager, form_id,
                                              filled_field_ids)) {
              MaybeLogCriticalAction(
                  critical_actions::ActionType::kFormFillContactInfo, *types);
            }
          },
          [&](const CreditCard*) {
            MaybeLogCriticalAction(
                critical_actions::ActionType::kFormFillPayments,
                {critical_actions::FormFillType::kCreditCard});
          },
          // Not supported yet.
          [](const EntityInstance*) {},
          // Not fills of saved data. OTPs are logged as `kCredentialsOtp`.
          [](const VerifiedProfile*) {},
          [](const OtpFillData*) {},
      },
      filling_payload);
}

void ActorAutofillManager::OnFillOrPreviewField(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    mojom::ActionPersistence action_persistence,
    const std::u16string& value,
    std::optional<FieldType> field_type_used) {
  if (action_persistence != mojom::ActionPersistence::kFill ||
      !field_type_used) {
    return;
  }

  // Single-field fills, e.g. one address field filled from a profile.
  if (std::optional<critical_actions::FormFillType> type =
          ToFormFillType(GroupTypeOfFieldType(*field_type_used))) {
    MaybeLogCriticalAction(critical_actions::GetFormFillGroup(*type), {*type});
  }
}

bool ActorAutofillManager::IsTabInActorMode() const {
  if (base::FeatureList::IsEnabled(features::debug::kAutofillForceActorMode)) {
    return true;
  }
  return active_actor_task_.has_value();
}

void ActorAutofillManager::OnActorTaskStateChanged(
    std::optional<ActorTaskInfo> task_info) {
  active_actor_task_ = std::move(task_info);
}

void ActorAutofillManager::MaybeLogCriticalAction(
    critical_actions::ActionType group,
    critical_actions::FormFillTypes form_fill_types) {
  if (!base::FeatureList::IsEnabled(
          critical_actions::features::kCriticalActionHistory)) {
    return;
  }

  if (!active_actor_task_) {
    return;  // Only log if the tab has an active Actor task.
  }

  if (active_actor_task_->skip_history_recording) {
    return;  // The task does not record history.
  }

  if (active_actor_task_->is_under_user_control) {
    return;  // The user is filling the form, not the actor.
  }

  if (!active_actor_task_->critical_action_logger_supports_task_source) {
    return;
  }

  if (!critical_action_service_) {
    return;
  }

  base::DictValue metadata_dict;
  critical_actions::SetFormFillTypes(metadata_dict, form_fill_types);

  critical_actions::CriticalActionEntry entry =
      critical_actions::CriticalActionEntry::Builder()
          .SetActionType(group)
          .SetActionSource(critical_actions::ActionSource::kAutofill)
          .SetConversationId(active_actor_task_->conversation_id)
          .SetActorTaskId(
              base::NumberToString(active_actor_task_->task_id.value()))
          .SetMetadata(base::WriteJson(metadata_dict).value_or(""))
          .Build();

  // Pass the navigation ID directly to the service. If the navigation ID is 0,
  // the CriticalActionService will handle the drop and record it into the
  // histogram.
  critical_action_service_->AddCriticalActionWithNavigationId(
      entry, client_->GetNavigationId());
}

}  // namespace autofill
