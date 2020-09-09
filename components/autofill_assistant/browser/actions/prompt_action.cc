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
#include "base/strings/string_number_conversions.h"
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
  callback_ = std::move(callback);
  if (proto_.prompt().choices_size() == 0) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  if (proto_.prompt().has_message()) {
    // TODO(b/144468818): Deprecate and remove message from this action and use
    // tell instead.
    delegate_->SetStatusMessage(proto_.prompt().message());
  }

  if (proto_.prompt().browse_mode()) {
    delegate_->SetBrowseDomainsWhitelist(
        {proto_.prompt().browse_domains_whitelist().begin(),
         proto_.prompt().browse_domains_whitelist().end()});
  }

  SetupConditions();
  UpdateUserActions();

  if (HasNonemptyPreconditions() || auto_select_ ||
      proto_.prompt().allow_interrupt()) {
    delegate_->WaitForDom(base::TimeDelta::Max(),
                          proto_.prompt().allow_interrupt(),
                          base::BindRepeating(&PromptAction::RegisterChecks,
                                              weak_ptr_factory_.GetWeakPtr()),
                          base::BindOnce(&PromptAction::OnDoneWaitForDom,
                                         weak_ptr_factory_.GetWeakPtr()));
  }
}

void PromptAction::RegisterChecks(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  if (!callback_) {
    // Action is done; checks aren't necessary anymore.
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  UpdateUserActions();

  for (size_t i = 0; i < preconditions_.size(); i++) {
    preconditions_[i]->Check(checker,
                             base::BindOnce(&PromptAction::OnPreconditionResult,
                                            weak_ptr_factory_.GetWeakPtr(), i));
  }

  if (auto_select_) {
    auto_select_->Check(checker,
                        base::BindOnce(&PromptAction::OnAutoSelectCondition,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
  checker->AddAllDoneCallback(base::BindOnce(&PromptAction::OnElementChecksDone,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(wait_for_dom_callback)));
}

void PromptAction::SetupConditions() {
  int choice_count = proto_.prompt().choices_size();
  preconditions_.resize(choice_count);
  precondition_results_.resize(choice_count);

  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    preconditions_[i] =
        std::make_unique<ElementPrecondition>(choice_proto.show_only_when());
    precondition_results_[i] = preconditions_[i]->empty();
  }

  ElementConditionsProto auto_select;
  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    if (choice_proto.has_auto_select_when()) {
      ElementConditionProto* condition = auto_select.add_conditions();
      *condition = choice_proto.auto_select_when();
      condition->set_payload(base::NumberToString(i));
    }
  }
  if (!auto_select.conditions().empty()) {
    ElementConditionProto auto_select_condition;
    *auto_select_condition.mutable_any_of() = auto_select;
    auto_select_ = std::make_unique<ElementPrecondition>(auto_select_condition);
  }
}

bool PromptAction::HasNonemptyPreconditions() {
  for (const auto& precondition : preconditions_) {
    if (!precondition->empty())
      return true;
  }
  return false;
}

void PromptAction::OnPreconditionResult(
    size_t choice_index,
    const ClientStatus& status,
    const std::vector<std::string>& ignored_payloads) {
  bool precondition_is_met = status.ok();
  if (precondition_results_[choice_index] == precondition_is_met)
    return;

  precondition_results_[choice_index] = precondition_is_met;
  precondition_changed_ = true;
}

void PromptAction::UpdateUserActions() {
  DCHECK(callback_);  // Make sure we're still waiting for a response

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    UserAction user_action(choice_proto.chip(), choice_proto.direct_action(),
                           /* enabled = */ true,
                           /* identifier = */ std::string());
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
  base::OnceCallback<void()> end_on_navigation_callback;
  if (proto_.prompt().end_on_navigation()) {
    end_on_navigation_callback = base::BindOnce(
        &PromptAction::OnNavigationEnded, weak_ptr_factory_.GetWeakPtr());
  }
  delegate_->Prompt(
      std::move(user_actions), proto_.prompt().disable_force_expand_sheet(),
      std::move(end_on_navigation_callback), proto_.prompt().browse_mode(),
      proto_.prompt().browse_mode_invisible());
  precondition_changed_ = false;
}

void PromptAction::OnAutoSelectCondition(
    const ClientStatus& status,
    const std::vector<std::string>& payloads) {
  if (payloads.empty())
    return;

  // We want to select the first matching choice, so only the first entry of
  // payloads matter.
  base::StringToInt(payloads[0], &auto_select_choice_index_);

  // Calling OnSuggestionChosen() is delayed until try_done, as it indirectly
  // deletes the batch element checker, which isn't supported from an element
  // check callback.
}

void PromptAction::OnElementChecksDone(
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  if (precondition_changed_)
    UpdateUserActions();

  // Calling wait_for_dom_callback with successful status is a way of asking the
  // WaitForDom to end gracefully and call OnDoneWaitForDom with the status.
  // Note that it is possible for WaitForDom to decide not to call
  // OnDoneWaitForDom, if an interrupt triggers at the same time, so we cannot
  // cancel the prompt and choose the suggestion just yet.
  if (auto_select_choice_index_ >= 0) {
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  // Let WaitForDom know we're still waiting for an element.
  std::move(wait_for_dom_callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED));
}

void PromptAction::OnDoneWaitForDom(const ClientStatus& status) {
  if (!callback_) {
    return;
  }
  // Status comes either from AutoSelectDone(), from checking the selector, or
  // from an interrupt failure. Special-case the AutoSelectDone() case.
  if (auto_select_choice_index_ >= 0) {
    OnSuggestionChosen(auto_select_choice_index_);
    return;
  }
  // Everything else should be forwarded.
  EndAction(status);
}

void PromptAction::OnSuggestionChosen(int choice_index) {
  if (!callback_) {
    NOTREACHED();
    return;
  }
  DCHECK(choice_index >= 0 && choice_index <= proto_.prompt().choices_size());

  processed_action_proto_->mutable_prompt_choice()->set_server_payload(
      proto_.prompt().choices(choice_index).server_payload());
  EndAction(ClientStatus(ACTION_APPLIED));
}

void PromptAction::OnNavigationEnded() {
  if (!callback_) {
    NOTREACHED();
    return;
  }
  processed_action_proto_->mutable_prompt_choice()->set_navigation_ended(true);
  EndAction(ClientStatus(ACTION_APPLIED));
}

void PromptAction::EndAction(const ClientStatus& status) {
  delegate_->CleanUpAfterPrompt();
  // Clear the whitelist when a browse action is done.
  if (proto_.prompt().browse_mode()) {
    delegate_->SetBrowseDomainsWhitelist({});
  }
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
