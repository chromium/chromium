// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {
namespace {

// Maximum amount of time normal actions should implicitly wait for a selector
// to show up.
constexpr base::TimeDelta kWaitForSelectorDeadline =
    base::TimeDelta::FromSeconds(2);
}  // namespace

ScriptExecutor::ScriptExecutor(const std::string& script_path,
                               const std::string& server_payload,
                               ScriptExecutor::Listener* listener,
                               ScriptExecutorDelegate* delegate)
    : script_path_(script_path),
      last_server_payload_(server_payload),
      listener_(listener),
      delegate_(delegate),
      at_end_(CONTINUE),
      should_stop_script_(false),
      should_clean_contextual_ui_on_finish_(false),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}
ScriptExecutor::~ScriptExecutor() {}

void ScriptExecutor::Run(RunScriptCallback callback) {
  callback_ = std::move(callback);
  DCHECK(delegate_->GetService());

  delegate_->GetService()->GetActions(
      script_path_, delegate_->GetWebController()->GetUrl(),
      delegate_->GetParameters(), last_server_payload_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<BatchElementChecker>
ScriptExecutor::CreateBatchElementChecker() {
  return delegate_->GetWebController()->CreateBatchElementChecker();
}

void ScriptExecutor::WaitForElement(const std::vector<std::string>& selectors,
                                    base::OnceCallback<void(bool)> callback) {
  std::unique_ptr<BatchElementChecker> checker = CreateBatchElementChecker();
  checker->AddElementCheck(kVisibilityCheck, selectors, base::DoNothing());
  checker->Run(kWaitForSelectorDeadline,
               /* try_done= */ base::DoNothing(),
               /* all_done= */
               base::BindOnce(
                   [](std::unique_ptr<BatchElementChecker> checker_to_delete,
                      base::OnceCallback<void(bool)> callback) {
                     std::move(callback).Run(checker_to_delete->all_found());
                   },
                   std::move(checker), std::move(callback)));
}

void ScriptExecutor::ShowStatusMessage(const std::string& message) {
  delegate_->GetUiController()->ShowStatusMessage(message);
}

void ScriptExecutor::ClickElement(const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->ClickElement(selectors, std::move(callback));
}

void ScriptExecutor::GetPaymentInformation(
    payments::mojom::PaymentOptionsPtr payment_options,
    base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
    const std::string& title) {
  delegate_->GetUiController()->GetPaymentInformation(
      std::move(payment_options), std::move(callback), title);
}

void ScriptExecutor::ChooseAddress(
    base::OnceCallback<void(const std::string&)> callback) {
  delegate_->GetUiController()->ChooseAddress(std::move(callback));
}

void ScriptExecutor::FillAddressForm(const std::string& guid,
                                     const std::vector<std::string>& selectors,
                                     base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FillAddressForm(guid, selectors,
                                                 std::move(callback));
}

void ScriptExecutor::ChooseCard(
    base::OnceCallback<void(const std::string&)> callback) {
  delegate_->GetUiController()->ChooseCard(std::move(callback));
}

void ScriptExecutor::FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                                  const base::string16& cvc,
                                  const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FillCardForm(std::move(card), cvc, selectors,
                                              std::move(callback));
}

void ScriptExecutor::SelectOption(const std::vector<std::string>& selectors,
                                  const std::string& selected_option,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SelectOption(selectors, selected_option,
                                              std::move(callback));
}

void ScriptExecutor::HighlightElement(const std::vector<std::string>& selectors,
                                      base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->HighlightElement(selectors,
                                                  std::move(callback));
}

void ScriptExecutor::FocusElement(const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FocusElement(selectors, std::move(callback));
}

void ScriptExecutor::ShowProgressBar(int progress, const std::string& message) {
  delegate_->GetUiController()->ShowProgressBar(progress, message);
}

void ScriptExecutor::HideProgressBar() {
  delegate_->GetUiController()->HideProgressBar();
}

void ScriptExecutor::ShowOverlay() {
  delegate_->GetUiController()->ShowOverlay();
}

void ScriptExecutor::HideOverlay() {
  delegate_->GetUiController()->HideOverlay();
}

void ScriptExecutor::SetFieldValue(const std::vector<std::string>& selectors,
                                   const std::string& value,
                                   bool simulate_key_presses,
                                   base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SetFieldValue(
      selectors, value, simulate_key_presses, std::move(callback));
}

void ScriptExecutor::GetOuterHtml(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  delegate_->GetWebController()->GetOuterHtml(selectors, std::move(callback));
}

void ScriptExecutor::LoadURL(const GURL& url) {
  delegate_->GetWebController()->LoadURL(url);
}

void ScriptExecutor::Shutdown() {
  at_end_ = SHUTDOWN;
}

void ScriptExecutor::Restart() {
  at_end_ = RESTART;
}

void ScriptExecutor::StopCurrentScript(const std::string& message) {
  if (!message.empty()) {
    delegate_->GetUiController()->ShowStatusMessage(message);
  }
  should_stop_script_ = true;
}

ClientMemory* ScriptExecutor::GetClientMemory() {
  return delegate_->GetClientMemory();
}

autofill::PersonalDataManager* ScriptExecutor::GetPersonalDataManager() {
  return delegate_->GetPersonalDataManager();
}

content::WebContents* ScriptExecutor::GetWebContents() {
  return delegate_->GetWebContents();
}

void ScriptExecutor::HideDetails() {
  delegate_->GetUiController()->HideDetails();
}

void ScriptExecutor::ShowDetails(const DetailsProto& details) {
  delegate_->GetUiController()->ShowDetails(details);
}

void ScriptExecutor::OnGetActions(bool result, const std::string& response) {
  if (!result) {
    RunCallback(false);
    return;
  }
  processed_actions_.clear();
  actions_.clear();

  bool parse_result =
      ProtocolUtils::ParseActions(response, &last_server_payload_, &actions_);
  if (listener_) {
    listener_->OnServerPayloadChanged(last_server_payload_);
  }
  if (!parse_result) {
    RunCallback(false);
    return;
  }

  if (actions_.empty()) {
    // Finished executing the script if there are no more actions.
    RunCallback(true);
    return;
  }

  ProcessNextAction();
}

void ScriptExecutor::RunCallback(bool success) {
  DCHECK(callback_);
  if (should_clean_contextual_ui_on_finish_ || !success) {
    HideDetails();
    should_clean_contextual_ui_on_finish_ = false;
  }

  ScriptExecutor::Result result;
  result.success = success;
  result.at_end = at_end_;
  std::move(callback_).Run(result);
}

void ScriptExecutor::ProcessNextAction() {
  // We could get into a strange situation if ProcessNextAction is called before
  // the action was reported as processed, which should not happen. In that case
  // we could have more |processed_actions| than |actions_|.
  if (actions_.size() <= processed_actions_.size()) {
    DCHECK_EQ(actions_.size(), processed_actions_.size());
    // Request more actions to execute.
    GetNextActions();
    return;
  }

  Action* action = actions_[processed_actions_.size()].get();
  should_clean_contextual_ui_on_finish_ = action->proto().clean_contextual_ui();
  int delay_ms = action->proto().action_delay_ms();
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScriptExecutor::ProcessAction,
                       weak_ptr_factory_.GetWeakPtr(), action),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    ProcessAction(action);
  }
}

void ScriptExecutor::ProcessAction(Action* action) {
  action->ProcessAction(this, base::BindOnce(&ScriptExecutor::OnProcessedAction,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::GetNextActions() {
  delegate_->GetService()->GetNextActions(
      last_server_payload_, processed_actions_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::OnProcessedAction(
    std::unique_ptr<ProcessedActionProto> processed_action_proto) {
  processed_actions_.emplace_back(*processed_action_proto);
  if (processed_actions_.back().status() !=
      ProcessedActionStatusProto::ACTION_APPLIED) {
    // Report error immediately, interrupting action processing.
    GetNextActions();
    return;
  }

  if (should_stop_script_) {
    // Last action called StopCurrentScript(). We simulate a successful end of
    // script to make sure we don't display any errors.
    RunCallback(true);
    return;
  }

  ProcessNextAction();
}

}  // namespace autofill_assistant
