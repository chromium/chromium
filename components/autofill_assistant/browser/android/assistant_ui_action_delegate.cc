// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_ui_action_delegate.h"

#include "base/logging.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/public/assistant_ui/proto/assistant_ui_action.pb.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

AssistantUiActionDelegate::AssistantUiActionDelegate() = default;
AssistantUiActionDelegate::~AssistantUiActionDelegate() = default;

void AssistantUiActionDelegate::OnActionRequested(
    const external::Action& action,
    bool is_interrupt,
    base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
    base::OnceCallback<void(const external::Result& result)>
        end_action_callback) {
  end_action_callback_ = std::move(end_action_callback);
  if (!action.info().has_assistant_ui_action()) {
    VLOG(1) << "Action is not of type AssistantUiAction";
    EndAction(false);
    return;
  }
  const assistant_ui::AssistantUiAction assistant_action =
      action.info().assistant_ui_action();

  if (assistant_action.has_update_ui()) {
    // TODO(b/242497041): Send UI update to Assistant.
  }

  assistant_ui::AssistantUiActionResult response;
  switch (assistant_action.continue_mode_case()) {
    case assistant_ui::AssistantUiAction::kBlockUntilUserAction:
      if (assistant_action.block_until_user_action().has_timeout_ms()) {
        timeout_timer_ = std::make_unique<base::OneShotTimer>();
        timeout_timer_->Start(
            FROM_HERE,
            base::Milliseconds(
                assistant_action.block_until_user_action().timeout_ms()),
            base::BindOnce(&AssistantUiActionDelegate::OnTimeout,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      if (assistant_action.block_until_user_action().check_dom_conditions()) {
        std::move(start_dom_checks_callback)
            .Run(base::BindRepeating(
                &AssistantUiActionDelegate::OnDomUpdateReceived,
                base::Unretained(this)));
      }
      return;
    case assistant_ui::AssistantUiAction::kContinueImmediately:
      response.set_immediate(true);
      EndAction(true, response);
      return;
    default:
      DLOG(ERROR)
          << "Assistant Ui External action didn't specify how to continue";
      EndAction(false);
  }
}
void AssistantUiActionDelegate::OnInterruptStarted() {
  // TODO(b/242497041): Implement interrupts.
  DCHECK(false) << "Interrupts aren't implemented yet";
}

void AssistantUiActionDelegate::OnInterruptFinished() {}
void AssistantUiActionDelegate::OnTimeout() {
  assistant_ui::AssistantUiActionResult response;
  response.set_timeout(true);
  EndAction(true, response);
}

void AssistantUiActionDelegate::OnDomUpdateReceived(
    const external::ElementConditionsUpdate& update) {
  assistant_ui::AssistantUiActionResult response;
  bool any_satisfied = false;
  for (const auto& condition : update.results()) {
    if (condition.satisfied()) {
      response.mutable_dom_conditions()->add_condition_id(condition.id());
      any_satisfied = true;
    }
  }
  if (any_satisfied) {
    EndAction(true, response);
  }
}

void AssistantUiActionDelegate::EndAction(
    bool success,
    absl::optional<assistant_ui::AssistantUiActionResult> action_result) {
  timeout_timer_.reset();
  external::Result result;
  result.set_success(success);

  if (action_result.has_value()) {
    *result.mutable_result_info()->mutable_assistant_ui_action_result() =
        action_result.value();
  }
  std::move(end_action_callback_).Run(std::move(result));
}

}  // namespace autofill_assistant
