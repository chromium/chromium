// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element.h"
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
    delegate_->SetBrowseDomainsAllowlist(
        {proto_.prompt().browse_domains_allowlist().begin(),
         proto_.prompt().browse_domains_allowlist().end()});
  }

  SetupConditions();
  UpdateUserActions();

  wait_time_stopwatch_.Start();
  if (HasNonemptyPreconditions() || auto_select_ ||
      proto_.prompt().allow_interrupt()) {
    // TODO(b/219004758): Enable observer-based WaitForDom, which would require
    // negating the preconditions that matched in the previous check, so that we
    // are alerted every time one changes instead of every time one becomes
    // true.
    delegate_->WaitForDom(
        /* max_wait_time= */ base::TimeDelta::Max(),
        /* allow_observer_mode = */ false, proto_.prompt().allow_interrupt(),
        /* observer= */ nullptr,
        base::BindRepeating(&PromptAction::RegisterChecks,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&PromptAction::OnWaitForElementTimed,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::BindOnce(&PromptAction::OnDoneWaitForDom,
                                      weak_ptr_factory_.GetWeakPtr())));
  }
}

void PromptAction::RegisterChecks(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  last_period_stopwatch_.Stop();
  last_checks_stopwatch_.Reset();
  last_checks_stopwatch_.Start();
  if (!callback_) {
    // Action is done; checks aren't necessary anymore.
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  UpdateUserActions();

  for (size_t i = 0; i < preconditions_.size(); i++) {
    checker->AddElementConditionCheck(
        preconditions_[i], base::BindOnce(&PromptAction::OnPreconditionResult,
                                          weak_ptr_factory_.GetWeakPtr(), i));
  }

  if (auto_select_) {
    checker->AddElementConditionCheck(
        auto_select_.value(),
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
  positive_precondition_changes_.resize(choice_count);
  precondition_stopwatches_.resize(choice_count);

  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    preconditions_[i] = choice_proto.show_only_when();
    precondition_results_[i] =
        BatchElementChecker::IsElementConditionEmpty(preconditions_[i]);
    positive_precondition_changes_[i] = false;
  }

  ElementConditionProto auto_select;
  auto* auto_select_any_of = auto_select.mutable_any_of();
  for (int i = 0; i < choice_count; i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    if (choice_proto.has_auto_select_when()) {
      ElementConditionProto* condition = auto_select_any_of->add_conditions();
      *condition = choice_proto.auto_select_when();
      condition->set_tag(base::NumberToString(i));
    }
  }
  if (!auto_select_any_of->conditions().empty()) {
    auto_select_ = auto_select;
  }
}

bool PromptAction::HasNonemptyPreconditions() {
  for (const auto& precondition : preconditions_) {
    if (!BatchElementChecker::IsElementConditionEmpty(precondition))
      return true;
  }
  return false;
}

void PromptAction::OnPreconditionResult(
    size_t choice_index,
    const ClientStatus& status,
    const std::vector<std::string>& ignored_payloads,
    const std::vector<std::string>& ignored_tags,
    const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements) {
  bool precondition_is_met = status.ok();
  if (precondition_results_[choice_index] == precondition_is_met)
    return;

  precondition_results_[choice_index] = precondition_is_met;
  positive_precondition_changes_[choice_index] = precondition_is_met;
  precondition_changed_ = true;
}

void PromptAction::UpdateUserActions() {
  DCHECK(callback_);  // Make sure we're still waiting for a response

  auto user_actions = std::make_unique<std::vector<UserAction>>();
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    auto& choice_proto = proto_.prompt().choices(i);
    UserAction user_action(choice_proto.chip(),
                           /* enabled = */ true,
                           /* identifier = */ std::string());
    if (!user_action.has_chip())
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

void PromptAction::UpdateTimings() {
  for (int i = 0; i < proto_.prompt().choices_size(); i++) {
    if (!precondition_results_[i]) {
      precondition_stopwatches_[i].Reset();
    } else if (positive_precondition_changes_[i]) {
      positive_precondition_changes_[i] = false;
      // The precondition now contains the duration of the wait for dom
      // operation retry period plus the time that it took to perform the actual
      // checks. We want to instead record only half of the period plus the
      // checks.
      precondition_stopwatches_[i].AddTime(last_checks_stopwatch_);
      precondition_stopwatches_[i].AddTime(
          last_period_stopwatch_.TotalElapsed() / 2);
    }
  }
}

void PromptAction::OnAutoSelectCondition(
    const ClientStatus& status,
    const std::vector<std::string>& payloads,
    const std::vector<std::string>& tags,
    const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements) {
  if (tags.empty())
    return;

  // We want to select the first matching choice, so only the first entry of
  // payloads matter.
  base::StringToInt(tags[0], &auto_select_choice_index_);

  // Calling OnSuggestionChosen() is delayed until try_done, as it indirectly
  // deletes the batch element checker, which isn't supported from an element
  // check callback.
}

void PromptAction::OnElementChecksDone(
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  last_checks_stopwatch_.Stop();
  if (precondition_changed_) {
    UpdateUserActions();
  }

  UpdateTimings();
  last_period_stopwatch_.Reset();
  last_period_stopwatch_.Start();

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

  if (auto_select_choice_index_ < 0) {
    wait_time_stopwatch_.Stop();
    action_stopwatch_.TransferToWaitTime(wait_time_stopwatch_.TotalElapsed());
    action_stopwatch_.TransferToActiveTime(
        precondition_stopwatches_[choice_index].TotalElapsed());
  }

  DCHECK(choice_index >= 0 && choice_index <= proto_.prompt().choices_size());

  processed_action_proto_->mutable_prompt_choice()->set_server_payload(
      proto_.prompt().choices(choice_index).server_payload());
  processed_action_proto_->mutable_prompt_choice()->set_choice_tag(
      proto_.prompt().choices(choice_index).tag());
  EndAction(ClientStatus(ACTION_APPLIED));
}

void PromptAction::OnNavigationEnded() {
  if (!callback_) {
    NOTREACHED();
    return;
  }
  action_stopwatch_.TransferToWaitTime(wait_time_stopwatch_.TotalElapsed());

  processed_action_proto_->mutable_prompt_choice()->set_navigation_ended(true);
  EndAction(ClientStatus(ACTION_APPLIED));
}

void PromptAction::EndAction(const ClientStatus& status) {
  delegate_->CleanUpAfterPrompt();
  // Clear the allowlist when a browse action is done.
  if (proto_.prompt().browse_mode()) {
    delegate_->SetBrowseDomainsAllowlist({});
  }
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
