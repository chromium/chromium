// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/wait_for_dom_operation.h"
#include "components/autofill_assistant/browser/actions/action_delegate_util.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "components/autofill_assistant/browser/web/element_action_util.h"

namespace autofill_assistant {

WaitForDomOperation::WaitForDomOperation(
    ScriptExecutor* main_script,
    ScriptExecutorDelegate* delegate,
    ScriptExecutorUiDelegate* ui_delegate,
    base::TimeDelta max_wait_time,
    bool allow_observers,
    bool allow_interrupt,
    WaitForDomObserver* observer,
    base::RepeatingCallback<void(BatchElementChecker*,
                                 base::OnceCallback<void(const ClientStatus&)>)>
        check_elements,
    WaitForDomOperation::Callback callback)
    : main_script_(main_script),
      delegate_(delegate),
      ui_delegate_(ui_delegate),
      max_wait_time_(max_wait_time),
      allow_interrupt_(allow_interrupt),
      use_observers_(allow_observers && delegate->GetTriggerContext()
                                            ->GetScriptParameters()
                                            .GetEnableObserverWaitForDom()
                                            .value_or(false)),
      observer_(observer),
      check_elements_(std::move(check_elements)),
      callback_(std::move(callback)),
      timeout_warning_delay_(delegate_->GetSettings().warning_delay),
      retry_timer_(delegate_->GetSettings().periodic_element_check_interval) {}

WaitForDomOperation::~WaitForDomOperation() {
  delegate_->RemoveNavigationListener(this);
}

void WaitForDomOperation::Run() {
  delegate_->AddNavigationListener(this);
  wait_time_stopwatch_.Start();
  Start();
}

void WaitForDomOperation::SetTimeoutWarningCallback(
    WarningCallback warning_callback) {
  warning_callback_ = std::move(warning_callback);
}

void WaitForDomOperation::Start() {
  retry_timer_.Start(
      max_wait_time_,
      base::BindRepeating(&WaitForDomOperation::RunChecks,
                          // safe since this instance owns retry_timer_
                          base::Unretained(this)),
      base::BindOnce(&WaitForDomOperation::RunCallback,
                     base::Unretained(this)));
}

void WaitForDomOperation::Pause() {
  if (interrupt_executor_) {
    // If an interrupt is running, it'll be the one to be paused, if necessary.
    return;
  }

  retry_timer_.Cancel();
}

void WaitForDomOperation::Continue() {
  if (retry_timer_.running() || !callback_)
    return;

  Start();
}

void WaitForDomOperation::OnNavigationStateChanged() {
  if (delegate_->IsNavigatingToNewDocument()) {
    Pause();
  } else {
    Continue();
  }
}

void WaitForDomOperation::OnServerPayloadChanged(
    const std::string& global_payload,
    const std::string& script_payload) {
  // Interrupts and main scripts share global payloads, but not script payloads.
  main_script_->last_global_payload_ = global_payload;
  main_script_->ReportPayloadsToListener();
}

void WaitForDomOperation::OnScriptListChanged(
    std::vector<std::unique_ptr<Script>> scripts) {
  main_script_->ReportScriptsUpdateToListener(std::move(scripts));
}

void WaitForDomOperation::TimeoutWarning() {
  if (warning_callback_) {
    std::move(warning_callback_)
        .Run(base::BindOnce(&WaitForDomOperation::SetSlowWarningStatus,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void WaitForDomOperation::SetSlowWarningStatus(bool was_shown) {
  if (was_shown) {
    warning_status_ = WARNING_SHOWN;
  } else {
    warning_status_ = WARNING_TRIGGERED;
  }
}

void WaitForDomOperation::RunChecks(
    base::OnceCallback<void(const ClientStatus&)> report_attempt_result) {
  warning_timer_ = std::make_unique<base::OneShotTimer>();
  warning_timer_->Start(FROM_HERE, timeout_warning_delay_,
                        base::BindOnce(&WaitForDomOperation::TimeoutWarning,
                                       weak_ptr_factory_.GetWeakPtr()));

  if (use_observers_) {
    // Observers should stop soon after the elements are in the page.
    wait_time_total_ = wait_time_stopwatch_.TotalElapsed();
  } else if (wait_time_stopwatch_.TotalElapsed() < retry_timer_.period()) {
    // It's the first run of the checks, set the total time waited to 0.
    wait_time_total_ = base::Seconds(0);
  } else {
    // If this is not the first run of the checks, in order to estimate
    // the real cost of periodic checks, half the duration of the retry
    // timer period is removed from the total wait time. This is to
    // account for the fact that the conditions could have been satisfied
    // at any point between the two consecutive checks.
    wait_time_total_ =
        wait_time_stopwatch_.TotalElapsed() - retry_timer_.period() / 2;
  }
  // Reset state possibly left over from previous runs.
  element_check_result_ = ClientStatus();
  runnable_interrupts_.clear();
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  check_elements_.Run(batch_element_checker_.get(),
                      base::BindOnce(&WaitForDomOperation::OnElementCheckDone,
                                     base::Unretained(this)));
  if (allow_interrupt_) {
    for (const std::unique_ptr<Script>& interrupt :
         *main_script_->ordered_interrupts_) {
      if (ran_interrupts_.find(interrupt->handle.path) !=
          ran_interrupts_.end()) {
        continue;
      }

      interrupt->precondition->Check(
          delegate_->GetCurrentURL(), batch_element_checker_.get(),
          *delegate_->GetTriggerContext(),
          base::BindOnce(&WaitForDomOperation::OnPreconditionCheckDone,
                         weak_ptr_factory_.GetWeakPtr(),
                         interrupt->handle.path));
    }
  }

  batch_element_checker_->AddAllDoneCallback(
      base::BindOnce(&WaitForDomOperation::OnAllChecksDone,
                     base::Unretained(this), std::move(report_attempt_result)));
  if (use_observers_) {
    batch_element_checker_->EnableObserver(
        {/* max_wait_time= */ max_wait_time_ -
             wait_time_stopwatch_.TotalElapsed(),
         /* min_check_interval= */
         delegate_->GetSettings().periodic_element_check_interval,
         /* extra_timeout= */
         delegate_->GetSettings().selector_observer_extra_timeout,
         /* debounce_interval */
         delegate_->GetSettings().selector_observer_debounce_interval});
  }
  batch_element_checker_->Run(delegate_->GetWebController());
}

void WaitForDomOperation::OnPreconditionCheckDone(
    const std::string& interrupt_path,
    bool precondition_match) {
  if (precondition_match)
    runnable_interrupts_.insert(interrupt_path);
}

void WaitForDomOperation::OnElementCheckDone(
    const ClientStatus& element_status) {
  element_check_result_ = element_status;

  // Wait for all checks to run before reporting that the element was found to
  // the caller, so interrupts have a chance to run.
}

void WaitForDomOperation::OnAllChecksDone(
    base::OnceCallback<void(const ClientStatus&)> report_attempt_result) {
  warning_timer_->Stop();
  if (runnable_interrupts_.empty()) {
    // Since no interrupts fired, allow previously-run interrupts to be run
    // again in the next round. This is meant to give elements one round to
    // disappear and avoid the simplest form of loops. A round with interrupts
    // firing doesn't count as one round here, because an interrupt can run
    // quickly and return immediately, without waiting for
    // periodic_element_check_interval.
    ran_interrupts_.clear();
  } else {
    // We must go through runnable_interrupts_ to make sure priority order is
    // respected in case more than one interrupt is ready to run.
    for (const std::unique_ptr<Script>& interrupt :
         *main_script_->ordered_interrupts_) {
      const std::string& path = interrupt->handle.path;
      if (runnable_interrupts_.find(path) != runnable_interrupts_.end()) {
        RunInterrupt(path);
        return;
      }
    }
  }
  std::move(report_attempt_result).Run(element_check_result_);
}

void WaitForDomOperation::RunInterrupt(const std::string& path) {
  batch_element_checker_.reset();
  if (observer_)
    observer_->OnInterruptStarted();

  SavePreInterruptState();
  ran_interrupts_.insert(path);
  interrupt_executor_ = std::make_unique<ScriptExecutor>(
      path,
      std::make_unique<TriggerContext>(std::vector<const TriggerContext*>{
          main_script_->additional_context_.get()}),
      main_script_->last_global_payload_, main_script_->initial_script_payload_,
      /* listener= */ this, &no_interrupts_, delegate_, ui_delegate_);
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  ui_delegate_->SetUserActions(nullptr);
  // Note that we don't clear the touchable area in the delegate here.
  // TODO(b/209732258): check whether this is a bug.
  interrupt_executor_->Run(main_script_->user_data_,
                           base::BindOnce(&WaitForDomOperation::OnInterruptDone,
                                          base::Unretained(this)));
  // base::Unretained(this) is safe because interrupt_executor_ belongs to this
}

void WaitForDomOperation::OnInterruptDone(
    const ScriptExecutor::Result& result) {
  interrupt_executor_.reset();
  if (!result.success || result.at_end != ScriptExecutor::CONTINUE) {
    RunCallbackWithResult(ClientStatus(INTERRUPT_FAILED), &result);
    return;
  }
  if (observer_)
    observer_->OnInterruptFinished();

  RestorePreInterruptState();
  RestorePreInterruptScroll();

  // Restart. We use the original wait time since the interruption could have
  // triggered any kind of actions, including actions that wait on the user. We
  // don't trust a previous element_found_ result, since it could have changed.
  Start();
}

void WaitForDomOperation::RunCallback(const ClientStatus& element_status) {
  RunCallbackWithResult(element_status, nullptr);
}

void WaitForDomOperation::RunCallbackWithResult(
    const ClientStatus& element_status,
    const ScriptExecutor::Result* result) {
  // stop element checking if one is still in progress
  batch_element_checker_.reset();
  retry_timer_.Cancel();
  warning_timer_->Stop();

  if (!callback_)
    return;

  ClientStatus status(element_status);
  status.set_slow_warning_status(warning_status_);

  std::move(callback_).Run(status, result, wait_time_total_);
}

void WaitForDomOperation::SavePreInterruptState() {
  if (saved_pre_interrupt_state_)
    return;

  ExecutorState pre_interrupt_state;
  pre_interrupt_state.status_message = ui_delegate_->GetStatusMessage();
  pre_interrupt_state.controller_state = delegate_->GetState();
  saved_pre_interrupt_state_ = pre_interrupt_state;
}

void WaitForDomOperation::RestorePreInterruptState() {
  if (!saved_pre_interrupt_state_)
    return;

  ui_delegate_->SetStatusMessage(saved_pre_interrupt_state_->status_message);
  delegate_->EnterState(saved_pre_interrupt_state_->controller_state);
  if (main_script_->touchable_element_area_) {
    delegate_->SetTouchableElementArea(*main_script_->touchable_element_area_);
  }
}

void WaitForDomOperation::RestorePreInterruptScroll() {
  if (!saved_pre_interrupt_state_)
    return;
  if (!main_script_->last_focused_element_.has_value())
    return;

  auto element = std::make_unique<ElementFinderResult>();
  if (!main_script_->GetElementStore()
           ->RestoreElement(*main_script_->last_focused_element_, element.get())
           .ok()) {
    return;
  }

  auto actions = std::make_unique<element_action_util::ElementActionVector>();
  action_delegate_util::AddStepIgnoreTiming(
      base::BindOnce(&ActionDelegate::WaitUntilDocumentIsInReadyState,
                     main_script_->GetWeakPtr(),
                     delegate_->GetSettings().document_ready_check_timeout,
                     DOCUMENT_INTERACTIVE),
      actions.get());
  actions->emplace_back(
      base::BindOnce(&WebController::ScrollIntoViewIfNeeded,
                     main_script_->GetWebController()->GetWeakPtr(),
                     /* center= */ true));
  element_action_util::TakeElementAndPerform(
      base::BindOnce(&element_action_util::PerformAll, std::move(actions)),
      /* done= */ base::DoNothing(), /* element_status= */ OkClientStatus(),
      std::move(element));
}

}  // namespace autofill_assistant
