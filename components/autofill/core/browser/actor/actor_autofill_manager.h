// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_AUTOFILL_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/actor/core/task_id.h"
#include "components/autofill/core/browser/actor/actor_key_metrics_recorder.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

namespace critical_actions {
class CriticalActionService;
}

namespace autofill {

class AutofillClient;

// Manages all actor-related autofill state and observers for a tab.
class ActorAutofillManager : public AutofillManager::Observer {
 public:
  struct ActorTaskInfo {
    std::string conversation_id;
    ::actor::TaskId task_id;
  };

  explicit ActorAutofillManager(
      AutofillClient* client,
      critical_actions::CriticalActionService* critical_action_service);
  ActorAutofillManager(const ActorAutofillManager&) = delete;
  ActorAutofillManager& operator=(const ActorAutofillManager&) = delete;
  ~ActorAutofillManager() override;

  // AutofillManager::Observer:
  void OnFillOrPreviewForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      FieldGlobalId trigger_field_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const base::flat_map<FieldGlobalId, DenseSet<FieldFillingSkipReason>>&,
      const FillingPayload& filling_payload) override;
  void OnFillOrPreviewField(AutofillManager& manager,
                            FormGlobalId form_id,
                            FieldGlobalId field_id,
                            mojom::ActionPersistence action_persistence,
                            const std::u16string& value,
                            std::optional<FieldType> field_type_used) override;

  // Returns whether the tab is in Actor mode.
  bool IsTabInActorMode() const;

  // The active actor task info, if any.
  std::optional<ActorTaskInfo> active_actor_task() const {
    return active_actor_task_;
  }
  void set_active_actor_task(std::optional<ActorTaskInfo> task_info) {
    active_actor_task_ = std::move(task_info);
  }

  ActorKeyMetricsRecorder& key_metrics_recorder() {
    return key_metrics_recorder_;
  }

 private:
  // Logs a critical action event to the CriticalActionService if the tab
  // has an active Actor task. Critical actions track actor-triggered
  // actions (such as form filling) to associate them with the user's
  // actor history.
  void MaybeLogCriticalAction(
      AutofillManager& manager,
      FormGlobalId form_id,
      const base::flat_set<FieldGlobalId>& filled_field_ids);

  const raw_ref<AutofillClient> client_;
  const raw_ptr<critical_actions::CriticalActionService>
      critical_action_service_;
  // Responsible for keeping track if (and which) actor is interacting with
  // the current tab. When present, some parts of Autofill may behave
  // differently. There can be at most one actor on a given tab. If there is no
  // actor interacting with the current tab it is `std::nullopt`.
  std::optional<ActorTaskInfo> active_actor_task_;

  ActorKeyMetricsRecorder key_metrics_recorder_;
  ScopedAutofillManagersObservation managers_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_ACTOR_AUTOFILL_MANAGER_H_
