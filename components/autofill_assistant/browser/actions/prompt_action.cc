// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_settings.h"
#include "components/autofill_assistant/browser/element_precondition.h"
#include "url/gurl.h"

namespace autofill_assistant {

PromptAction::PromptAction(ActionDelegate* delegate, const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_prompt());
}

PromptAction::~PromptAction() {}

void PromptAction::InternalProcessAction(ProcessActionCallback callback) {
  if (proto_.prompt().choices_size() == 0) {
    UpdateProcessedAction(INVALID_ACTION);
    std::move(callback).Run(std::move(processed_action_proto_));
    return;
  }

  callback_ = std::move(callback);
  if (proto_.prompt().has_message()) {
    // TODO(b/144468818): Deprecate and remove message from this action and use
    // tell instead.
    delegate_->SetStatusMessage(proto_.prompt().message());
  }

  SetupPreconditions();
  UpdateUserActions();

  if (HasNonemptyPreconditions() || HasAutoSelect()) {
    RunPeriodicChecks();
    timer_ = std::make_unique<base::RepeatingTimer>();
    timer_->Start(FROM_HERE,
                  delegate_->GetSettings().periodic_script_check_interval,
                  base::BindRepeating(&PromptAction::RunPeriodicChecks,
                                      weak_ptr_factory_.GetWeakPtr()));
  }
}

void PromptAction::RunPeriodicChecks() {
  CheckPreconditions();
  CheckAutoSelect();
}

void PromptAction::SetupPreconditions() {
  int choice_count = proto_.prompt().choices_size();
  preconditions_.resize(choice_count);
  precondition_results_.resize(choice_count);
  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    preconditions_[i] = std::make_unique<ElementPrecondition>(
        choice_proto.show_only_if_element_exists(),
        choice_proto.show_only_if_form_value_matches());
    precondition_results_[i] = preconditions_[i]->empty();
  }
}

bool PromptAction::HasNonemptyPreconditions() {
  for (const auto& precondition : preconditions_) {
    if (!precondition->empty())
      return true;
  }
  return false;
}

void PromptAction::CheckPreconditions() {
  precondition_checker_ = std::make_unique<BatchElementChecker>();
  for (size_t i = 0; i < preconditions_.size(); i++) {
    preconditions_[i]->Check(precondition_checker_.get(),
                             base::BindOnce(&PromptAction::OnPreconditionResult,
                                            weak_ptr_factory_.GetWeakPtr(), i));
  }
  precondition_checker_->AddAllDoneCallback(base::BindOnce(
      &PromptAction::OnPreconditionChecksDone, weak_ptr_factory_.GetWeakPtr()));
  delegate_->RunElementChecks(precondition_checker_.get());
}

void PromptAction::OnPreconditionResult(size_t choice_index, bool result) {
  if (precondition_results_[choice_index] == result)
    return;

  precondition_results_[choice_index] = result;
  precondition_changed_ = true;
}

void PromptAction::OnPreconditionChecksDone() {
  if (precondition_changed_)
    UpdateUserActions();
}

void PromptAction::UpdateUserActions() {
  DCHECK(callback_);  // Make sure we're still waiting for a response

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    UserAction user_action(choice_proto.chip(), choice_proto.direct_action());
    if (!user_action.has_triggers())
      continue;

    // Hide actions whose preconditions don't match.
    if (!precondition_results_[i] && !choice_proto.allow_disabling())
      continue;

    user_action.SetEnabled(precondition_results_[i]);
    user_action.SetCallback(base::BindOnce(&PromptAction::OnSuggestionChosen,
                                           weak_ptr_factory_.GetWeakPtr(), i));
    user_actions->emplace_back(std::move(user_action));
  }
  delegate_->Prompt(std::move(user_actions));
  precondition_changed_ = false;
}

bool PromptAction::HasAutoSelect() {
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    Selector selector =
        Selector(proto_.prompt().choices(i).auto_select_if_element_exists());
    if (!selector.empty())
      return true;
  }
  return false;
}

void PromptAction::CheckAutoSelect() {
  auto_select_checker_ = std::make_unique<BatchElementChecker>();

  // Wait as long as necessary for one of the elements to show up. This is
  // cancelled by CancelPrompt()
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    Selector selector =
        Selector(proto_.prompt().choices(i).auto_select_if_element_exists());
    if (selector.empty())
      continue;

    auto_select_checker_->AddElementCheck(
        selector, base::BindOnce(&PromptAction::OnAutoSelectElementExists,
                                 weak_ptr_factory_.GetWeakPtr(), i));
  }
  auto_select_checker_->AddAllDoneCallback(base::BindOnce(
      &PromptAction::OnAutoSelectDone, weak_ptr_factory_.GetWeakPtr()));
  delegate_->RunElementChecks(auto_select_checker_.get());
}

void PromptAction::OnAutoSelectElementExists(
    int choice_index,
    const ClientStatus& element_status) {
  if (element_status.ok())
    auto_select_choice_index_ = choice_index;

  // Calling OnSuggestionChosen() is delayed until try_done, as it indirectly
  // deletes the batch element checker, which isn't supported from an element
  // check callback.
}

void PromptAction::OnAutoSelectDone() {
  if (auto_select_choice_index_ >= 0) {
    delegate_->CancelPrompt();
    OnSuggestionChosen(auto_select_choice_index_);
  }
}

void PromptAction::OnSuggestionChosen(int choice_index) {
  if (!callback_) {
    NOTREACHED();
    return;
  }
  DCHECK(choice_index >= 0 && choice_index <= proto_.prompt().choices_size());

  // Interrupt checks and timer.
  timer_.reset();
  precondition_checker_.reset();
  auto_select_checker_.reset();

  PromptProto::Choice choice;
  UpdateProcessedAction(ACTION_APPLIED);
  *processed_action_proto_->mutable_prompt_choice() =
      proto_.prompt().choices(choice_index);
  std::move(callback_).Run(std::move(processed_action_proto_));
}
}  // namespace autofill_assistant
