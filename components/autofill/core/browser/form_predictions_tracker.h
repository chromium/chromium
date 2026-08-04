// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_H_

#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

// Detects if all forms on the given tab were parsed by the local heuristics and
// the server.
class FormPredictionsTracker : public AutofillManager::Observer {
 public:
  explicit FormPredictionsTracker(AutofillClient* client);
  ~FormPredictionsTracker() override;

  // Inserts `callback` into `callbacks_`. The callbacks are executed once all
  // forms on the current tab have been parsed by the local heuristics and the
  // server, or when more than `timeout` time has passed since starting to wait.
  virtual void Wait(base::OnceClosure callback, base::TimeDelta timeout);

 private:
  friend class FormPredictionsTrackerTestApi;

  // Verifies that all forms got predictions and executes all callbacks in
  // `callbacks_`, then clears `callbacks_`.
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
  void OnBeforeLoadedServerPredictions(
      AutofillManager& manager,
      base::span<const FormGlobalId> forms) override;
  void OnAfterLoadedServerPredictions(
      AutofillManager& manager,
      base::span<const FormGlobalId> forms) override;

  // Maps a form identifier to the number of started but not completed
  // operations. This enables support for interleaved parsing operations.
  // E.g.
  // OnBeforeFormsSeen({f}, {});  // with `!small_forms_were_parsed`
  // OnBeforeFormsSeen({f}, {});  // with `small_forms_were_parsed`
  // OnAfterFormsSeen({f}, {});  // with `!small_forms_were_parsed`
  // Now forms_in_parsing_state_ would contain (f, 1) as one more
  // OnAfterFormsSeen call is pending.
  absl::flat_hash_map<FormGlobalId, int> forms_in_parsing_state_;
  absl::flat_hash_map<FormGlobalId, int> forms_awaiting_server_response_;

  // Callbacks that inform callers that form parsing is complete or that the
  // timeout has been reached.
  std::vector<base::OnceClosure> callbacks_;

  // The client that owns `this`.
  const raw_ptr<AutofillClient> client_;

  // The observation for the Autofill manager of the relevant tab.
  ScopedAutofillManagersObservation autofill_managers_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PREDICTIONS_TRACKER_H_
