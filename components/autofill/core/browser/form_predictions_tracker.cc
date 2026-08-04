// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_predictions_tracker.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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

void EraseAll(absl::flat_hash_map<FormGlobalId, int>& map,
              base::span<const FormGlobalId> forms_to_erase) {
  for (const FormGlobalId& form_id : forms_to_erase) {
    map.erase(form_id);
  }
}

// Increments the number of times `RegisterCompletedWork` needs to be
// called for `forms` such that all work is considered completed.
void RegisterPendingWork(absl::flat_hash_map<FormGlobalId, int>& map,
                         base::span<const FormGlobalId> forms) {
  for (const FormGlobalId& form_id : forms) {
    map[form_id]++;
  }
}

// Decrements the number of times `RegisterCompletedWork` needs to be
// called for `forms` such that all work is considered completed and
// clears entries without pending work.
void RegisterCompletedWork(absl::flat_hash_map<FormGlobalId, int>& map,
                           base::span<const FormGlobalId> forms) {
  for (const FormGlobalId& form_id : forms) {
    if (auto iter = map.find(form_id); iter != map.end()) {
      iter->second--;
      if (iter->second == 0) {
        map.erase(iter);
      }
    }
  }
}

}  // namespace

FormPredictionsTracker::FormPredictionsTracker(AutofillClient* client)
    : client_(client) {
  CHECK(client);
  autofill_managers_observation_.Observe(client);
}

FormPredictionsTracker::~FormPredictionsTracker() = default;

void FormPredictionsTracker::Wait(base::OnceClosure callback,
                                  base::TimeDelta timeout) {
  if (!client_->IsTabInActorMode() ||
      !base::FeatureList::IsEnabled(
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
  bool all_forms_parsed = forms_in_parsing_state_.empty() &&
                          forms_awaiting_server_response_.empty();
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
    LocalFrameToken local_frame_token = manager.driver().GetFrameToken();
    auto matcher = [local_frame_token](const auto& item) {
      return item.first.frame_token == local_frame_token;
    };
    absl::erase_if(forms_in_parsing_state_, matcher);
    absl::erase_if(forms_awaiting_server_response_, matcher);

    MaybeNotifyWaitingCallbacks();
  }
}

void FormPredictionsTracker::OnBeforeFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  // These are deleted in OnAfterFormsSeen instead of here to deal with the
  // following order of events.
  // OnBeforeFormsSeen({f}, {});
  // OnBeforeFormsSeen({}, {f});
  // OnAfterFormsSeen({f}, {});
  // OnAfterFormsSeen({}, {f});
  RegisterPendingWork(forms_in_parsing_state_, updated_forms);
}

void FormPredictionsTracker::OnAfterFormsSeen(
    AutofillManager& manager,
    base::span<const FormGlobalId> updated_forms,
    base::span<const FormGlobalId> removed_forms) {
  EraseAll(forms_in_parsing_state_, removed_forms);
  EraseAll(forms_awaiting_server_response_, removed_forms);
  RegisterCompletedWork(forms_in_parsing_state_, updated_forms);
  MaybeNotifyWaitingCallbacks();
}

void FormPredictionsTracker::OnBeforeLoadedServerPredictions(
    AutofillManager& manager,
    base::span<const FormGlobalId> forms) {
  RegisterPendingWork(forms_awaiting_server_response_, forms);
}

void FormPredictionsTracker::OnAfterLoadedServerPredictions(
    AutofillManager& manager,
    base::span<const FormGlobalId> forms) {
  RegisterCompletedWork(forms_awaiting_server_response_, forms);
  MaybeNotifyWaitingCallbacks();
}

}  // namespace autofill
