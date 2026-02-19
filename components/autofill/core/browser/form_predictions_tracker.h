// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

// Detects if all forms on the given tab were parsed by the local heuristics and
// the server in the actor mode.
// TODO(crbug.com/485547157): Make this class more generic and wait for
// non-actor mode predictions too.
class FormPredictionsTracker : public AutofillManager::Observer {
 public:
  explicit FormPredictionsTracker(AutofillClient* client);
  ~FormPredictionsTracker() override;

  struct FormParsingStatus {
    bool server_predicted_in_actor_mode = false;
    bool heuristic_parsed_in_actor_mode = false;
  };

  // Inserts `callback` to `callbacks_`. It will be executed once all forms on
  // the current tab are parsed in the actor mode, or more than `timeout` passed
  // since starting to wait.
  virtual void Wait(base::OnceClosure callback, base::TimeDelta timeout);

 private:
  friend class FormPredictionsTrackerTestApi;

  // Verifies that all forms got predictions in actor mode and executes all
  // callbacks in `callbacks_`, then clears `callbacks_`.
  void MaybeNotifyWaitingCallbacks();

  // AutofillManager::Observer:
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillDriver::LifecycleState old_state,
      AutofillDriver::LifecycleState new_state) override;
  void OnBeforeFormsSeen(AutofillManager& manager,
                         base::span<const FormGlobalId> updated_forms,
                         base::span<const FormGlobalId> removed_forms) override;
  void OnAfterFormsSeen(AutofillManager& manager,
                        base::span<const FormGlobalId> updated_forms,
                        base::span<const FormGlobalId> removed_forms) override;
  void OnFieldTypesDetermined(autofill::AutofillManager& manager,
                              autofill::FormGlobalId form_id,
                              FieldTypeSource source,
                              bool small_forms_were_parsed) override;

  // Keeps track of all of the forms in the current tab and their parsing
  // status.
  absl::flat_hash_map<FormGlobalId, FormParsingStatus> form_parsing_status_;

  // Callbacks that inform callers that form parsing is complete or that the
  // timeout has been reached.
  std::vector<base::OnceClosure> callbacks_;

  // The observation for the Autofill manager of the relevant tab.
  ScopedAutofillManagersObservation autofill_managers_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_H_
