// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOM_OPERATION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOM_OPERATION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/stopwatch.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/script_executor_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/state.h"

namespace autofill_assistant {

class WaitForDomObserver;

// Helper that keeps track of the state required to run interrupts while waiting
// for a specific element.
class WaitForDomOperation : public ScriptExecutor::Listener,
                            ScriptExecutorDelegate::NavigationListener {
 public:
  // Let the caller know about either the result of looking for the element or
  // of an abnormal result from an interrupt.
  //
  // If the given result is non-null, it should be forwarded as the result of
  // the main script.
  using Callback = base::OnceCallback<void(const ClientStatus&,
                                           const ScriptExecutor::Result*,
                                           base::TimeDelta)>;

  using WarningCallback =
      base::OnceCallback<void(base::OnceCallback<void(bool)>)>;

  // |main_script_| must not be null and outlive this instance.
  WaitForDomOperation(
      ScriptExecutor* main_script,
      ScriptExecutorDelegate* delegate,
      ScriptExecutorUiDelegate* ui_delegate,
      base::TimeDelta max_wait_time,
      bool allow_observer_mode,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      WaitForDomOperation::Callback callback);

  WaitForDomOperation(const WaitForDomOperation&) = delete;
  WaitForDomOperation& operator=(const WaitForDomOperation&) = delete;

  ~WaitForDomOperation() override;

  void Run();
  void Terminate();
  void SetTimeoutWarningCallback(WarningCallback timeout_warning);

 private:
  struct ExecutorState {
    // The status message that was displayed when the interrupt started.
    std::string status_message;
    // The state the controller was in when the interrupt triggered.
    AutofillAssistantState controller_state;
  };

  void Start();
  void Pause();
  void Continue();

  // Implements ScriptExecutorDelegate::NavigationListener
  void OnNavigationStateChanged() override;

  // Implements ScriptExecutor::Listener
  void OnServerPayloadChanged(const std::string& global_payload,
                              const std::string& script_payload) override;
  void OnScriptListChanged(
      std::vector<std::unique_ptr<Script>> scripts) override;

  void RunChecks(
      base::OnceCallback<void(const ClientStatus&)> report_attempt_result);
  void OnPreconditionCheckDone(const std::string& interrupt_path,
                               bool precondition_match);
  void OnElementCheckDone(const ClientStatus&);
  void OnAllChecksDone(
      base::OnceCallback<void(const ClientStatus&)> report_attempt_result);
  void RunInterrupt(const std::string& path);
  void OnInterruptDone(const ScriptExecutor::Result& result);
  void RunCallback(const ClientStatus& element_status);
  void RunCallbackWithResult(const ClientStatus& element_status,
                             const ScriptExecutor::Result* result);
  void SetSlowWarningStatus(bool was_shown);

  // Saves the current state and sets save_pre_interrupt_state_.
  void SavePreInterruptState();

  // Restores the state as found by SavePreInterruptState.
  void RestorePreInterruptState();

  // if save_pre_interrupt_state_ is set, attempt to scroll the page back to
  // the original area.
  void RestorePreInterruptScroll();

  void TimeoutWarning();

  raw_ptr<ScriptExecutor> main_script_;
  raw_ptr<ScriptExecutorDelegate> delegate_;
  raw_ptr<ScriptExecutorUiDelegate> ui_delegate_;
  const base::TimeDelta max_wait_time_;
  const bool allow_interrupt_;
  const bool use_observers_;
  raw_ptr<WaitForDomObserver> observer_;
  base::RepeatingCallback<void(BatchElementChecker*,
                               base::OnceCallback<void(const ClientStatus&)>)>
      check_elements_;
  WaitForDomOperation::Callback callback_;
  base::OnceCallback<void(base::OnceCallback<void(bool)>)> warning_callback_;
  std::unique_ptr<base::OneShotTimer> warning_timer_;
  base::TimeDelta timeout_warning_delay_;

  SlowWarningStatus warning_status_ = NO_WARNING;

  std::unique_ptr<BatchElementChecker> batch_element_checker_;

  // Path of interrupts from |ordered_interrupts_| that have been found
  // runnable.
  std::set<std::string> runnable_interrupts_;
  ClientStatus element_check_result_;

  // An empty vector of interrupts that can be passed to interrupt_executor_
  // and outlives it. Interrupts must not run interrupts.
  const std::vector<std::unique_ptr<Script>> no_interrupts_;
  Stopwatch wait_time_stopwatch_;
  Stopwatch period_stopwatch_;
  base::TimeDelta wait_time_total_;

  // The interrupt that's currently running.
  std::unique_ptr<ScriptExecutor> interrupt_executor_;

  // The state of the ScriptExecutor, as registered before the first interrupt
  // is run.
  absl::optional<ExecutorState> saved_pre_interrupt_state_;

  // Paths of the interrupts that were just run. These interrupts are
  // prevented from firing for one round.
  std::set<std::string> ran_interrupts_;

  RetryTimer retry_timer_;

  base::WeakPtrFactory<WaitForDomOperation> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_WAIT_FOR_DOM_OPERATION_H_
