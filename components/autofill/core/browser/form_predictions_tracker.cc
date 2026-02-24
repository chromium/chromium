// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_predictions_tracker.h"

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {
namespace {
// Runs `cb` after `timeout` or if `Run()` is called.
class TimeoutHelper {
 public:
  TimeoutHelper(base::OnceClosure cb, base::TimeDelta timeout)
      : cb_(std::move(cb)) {
    timer_.Start(FROM_HERE, timeout, this, &TimeoutHelper::Run);
  }
  ~TimeoutHelper() = default;

  void Run() {
    if (base::OnceClosure cb = std::exchange(cb_, {})) {
      std::move(cb).Run();
    }
  }

 private:
  base::OnceClosure cb_;
  base::OneShotTimer timer_;
};

base::OnceClosure WrapAsTimeoutCallback(base::OnceClosure cb,
                                        base::TimeDelta timeout) {
  auto helper = std::make_unique<TimeoutHelper>(std::move(cb), timeout);
  return base::BindOnce(&TimeoutHelper::Run, std::move(helper));
}

}  // namespace

FormPredictionsTracker::FormPredictionsTracker(AutofillClient* client) {
  CHECK(client);
  autofill_managers_observation_.Observe(client);
}

FormPredictionsTracker::~FormPredictionsTracker() = default;

void FormPredictionsTracker::Wait(base::OnceClosure callback,
                                  base::TimeDelta timeout) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDelayApcForPredictions)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  callbacks_.push_back(WrapAsTimeoutCallback(std::move(callback), timeout));

  // It may happen that forms were parsed before the waiting was requested.
  MaybeNotifyWaitingCallbacks();
}

void FormPredictionsTracker::MaybeNotifyWaitingCallbacks() {
  if (callbacks_.empty()) {
    return;
  }

  // TODO(crbug.com/479794574): Do not wait for empty forms when notifing
  // `ObservationDelayController`
  bool all_forms_parsed =
      std::ranges::all_of(form_parsing_status_, [](const auto& pair) {
        return pair.second.server_predicted_in_actor_mode &&
               pair.second.heuristic_parsed_in_actor_mode;
      });
  if (all_forms_parsed) {
    for (base::OnceClosure& callback : std::exchange(callbacks_, {})) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(callback));
    }
  }
}

void FormPredictionsTracker::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillDriver::LifecycleState old_state,
    AutofillDriver::LifecycleState new_state) {
  if (new_state == AutofillDriver::LifecycleState::kPendingReset ||
      new_state == AutofillDriver::LifecycleState::kPendingDeletion) {
    autofill::LocalFrameToken local_frame_token =
        manager.driver().GetFrameToken();
    absl::erase_if(form_parsing_status_, [local_frame_token](const auto& pair) {
      return pair.first.frame_token == local_frame_token;
    });

    MaybeNotifyWaitingCallbacks();
  }
}

void FormPredictionsTracker::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  // Insert new forms or invalidate the parsing state of modified forms.
  for (FormGlobalId form_global_id : updated_forms) {
    form_parsing_status_[form_global_id] = FormParsingStatus();
  }
  for (const FormGlobalId& form_global_id : removed_forms) {
    form_parsing_status_.erase(form_global_id);
  }

  MaybeNotifyWaitingCallbacks();
}

void FormPredictionsTracker::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  // If a form was seen as updated in `OnBeforeFormsSeen()`, but `manager` does
  // not own it here, it means that it didn't satisfy the requirements for
  // parsing it (e.g. had 0 fields). In that case, this class should not wait
  // for it.
  for (const FormGlobalId form_id : updated_forms) {
    if (!manager.FindCachedFormById(form_id)) {
      // Form was not actually parsed.
      form_parsing_status_.erase(form_id);
    }
  }

  MaybeNotifyWaitingCallbacks();
}

void FormPredictionsTracker::OnFieldTypesDetermined(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    FieldTypeSource source,
    bool small_forms_were_parsed) {
  if (!small_forms_were_parsed) {
    return;
  }
  // Parsing might finish after the form got removed from the DOM.
  if (!form_parsing_status_.contains(form_id)) {
    return;
  }

  switch (source) {
    case FieldTypeSource::kHeuristicsOrAutocomplete:
      form_parsing_status_[form_id].heuristic_parsed_in_actor_mode = true;
      break;
    case FieldTypeSource::kAutofillServer:
      form_parsing_status_[form_id].server_predicted_in_actor_mode = true;
      break;
    case FieldTypeSource::kAutofillAiModel:
      // Not supported by GLIC.
      break;
  }

  MaybeNotifyWaitingCallbacks();
}

}  // namespace autofill
