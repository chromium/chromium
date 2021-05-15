// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

namespace {

void WriteCreditCardsToUserModel(
    std::unique_ptr<std::vector<std::unique_ptr<autofill::CreditCard>>>
        credit_cards,
    const ShowGenericUiProto::RequestAutofillCreditCards& proto,
    UserModel* user_model) {
  DCHECK(credit_cards);
  DCHECK(user_model);
  ValueProto model_value;
  model_value.set_is_client_side_only(true);
  for (const auto& credit_card : *credit_cards) {
    DCHECK(!credit_card->guid().empty());
    model_value.mutable_credit_cards()->add_values()->set_guid(
        credit_card->guid());
  }
  user_model->SetAutofillCreditCards(std::move(credit_cards));
  user_model->SetValue(proto.model_identifier(), model_value);
}

void WriteProfilesToUserModel(
    std::unique_ptr<std::vector<std::unique_ptr<autofill::AutofillProfile>>>
        profiles,
    const ShowGenericUiProto::RequestAutofillProfiles& proto,
    UserModel* user_model) {
  DCHECK(profiles);
  DCHECK(user_model);
  ValueProto model_value;
  model_value.set_is_client_side_only(true);
  for (const auto& profile : *profiles) {
    DCHECK(!profile->guid().empty());
    model_value.mutable_profiles()->add_values()->set_guid(profile->guid());
  }
  user_model->SetAutofillProfiles(std::move(profiles));
  user_model->SetValue(proto.model_identifier(), model_value);
}

void WriteLoginOptionsToUserModel(
    const ShowGenericUiProto::RequestLoginOptions& proto,
    UserModel* user_model,
    std::vector<WebsiteLoginManager::Login> logins) {
  DCHECK(user_model);
  ValueProto model_value;
  model_value.set_is_client_side_only(true);
  for (const auto& login_option : proto.login_options()) {
    switch (login_option.type_case()) {
      case ShowGenericUiProto::RequestLoginOptions::LoginOption::
          kCustomLoginOption:
        *model_value.mutable_login_options()->add_values() =
            login_option.custom_login_option();
        break;
      case ShowGenericUiProto::RequestLoginOptions::LoginOption::
          kPasswordManagerLogins: {
        for (const auto& login : logins) {
          auto* option = model_value.mutable_login_options()->add_values();
          option->set_label(login.username);
          option->set_sublabel(
              login_option.password_manager_logins().sublabel());
          option->set_payload(login_option.password_manager_logins().payload());
        }
        break;
      }
      case ShowGenericUiProto::RequestLoginOptions::LoginOption::TYPE_NOT_SET:
        NOTREACHED();
        break;
    }
  }
  user_model->SetValue(proto.model_identifier(), model_value);
}
}  // namespace

void ShowGenericUiAction::OnInterruptStarted() {
  delegate_->GetPersonalDataManager()->RemoveObserver(this);
  delegate_->ClearGenericUi();
}

void ShowGenericUiAction::OnInterruptFinished() {
  delegate_->SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(
          proto_.show_generic_ui().generic_user_interface()),
      base::BindOnce(&ShowGenericUiAction::OnEndActionInteraction,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ShowGenericUiAction::OnViewInflationFinished,
                     weak_ptr_factory_.GetWeakPtr(), false));
}

ShowGenericUiAction::ShowGenericUiAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_generic_ui());
}

ShowGenericUiAction::~ShowGenericUiAction() {
  delegate_->GetPersonalDataManager()->RemoveObserver(this);
}

bool ShowGenericUiAction::ShouldInterruptOnPause() const {
  return true;
}

void ShowGenericUiAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  // Check that |output_model_identifiers| is a subset of input model.
  UserModel temp_model;
  temp_model.MergeWithProto(
      proto_.show_generic_ui().generic_user_interface().model(),
      /* force_notifications = */ false);
  if (!temp_model.GetValues(proto_.show_generic_ui().output_model_identifiers())
           .has_value()) {
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  for (const auto& element_check :
       proto_.show_generic_ui().periodic_element_checks().element_checks()) {
    if (element_check.model_identifier().empty()) {
      VLOG(1) << "Invalid action: ElementCheck with empty model_identifier";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
  }
  for (const auto& additional_value :
       proto_.show_generic_ui().request_user_data().additional_values()) {
    if (!delegate_->GetUserData()->has_additional_value(
            additional_value.source_identifier())) {
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
  }
  for (const auto& additional_value :
       proto_.show_generic_ui().request_user_data().additional_values()) {
    ValueProto value = *delegate_->GetUserData()->additional_value(
        additional_value.source_identifier());
    value.set_is_client_side_only(true);
    delegate_->GetUserModel()->SetValue(additional_value.model_identifier(),
                                        value);
  }
  if (proto_.show_generic_ui().has_request_login_options()) {
    auto login_options =
        proto_.show_generic_ui().request_login_options().login_options();
    if (std::find_if(login_options.begin(), login_options.end(),
                     [&](const auto& option) {
                       return option.type_case() ==
                              ShowGenericUiProto::RequestLoginOptions::
                                  LoginOption::kPasswordManagerLogins;
                     }) != login_options.end()) {
      delegate_->GetWebsiteLoginManager()->GetLoginsForUrl(
          delegate_->GetWebContents()->GetLastCommittedURL(),
          base::BindOnce(&WriteLoginOptionsToUserModel,
                         proto_.show_generic_ui().request_login_options(),
                         delegate_->GetUserModel()));
    } else {
      WriteLoginOptionsToUserModel(
          proto_.show_generic_ui().request_login_options(),
          delegate_->GetUserModel(),
          /* logins = */ std::vector<WebsiteLoginManager::Login>());
    }
  }

  base::OnceCallback<void()> end_on_navigation_callback;
  if (proto_.show_generic_ui().end_on_navigation()) {
    end_on_navigation_callback =
        base::BindOnce(&ShowGenericUiAction::OnNavigationEnded,
                       weak_ptr_factory_.GetWeakPtr());
  }
  delegate_->Prompt(/* user_actions = */ nullptr,
                    /* disable_force_expand_sheet = */ false,
                    std::move(end_on_navigation_callback));
  delegate_->SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(
          proto_.show_generic_ui().generic_user_interface()),
      base::BindOnce(&ShowGenericUiAction::OnEndActionInteraction,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ShowGenericUiAction::OnViewInflationFinished,
                     weak_ptr_factory_.GetWeakPtr(),
                     /* first_inflation= */ true));
}

void ShowGenericUiAction::OnViewInflationFinished(bool first_inflation,
                                                  const ClientStatus& status) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  delegate_->GetPersonalDataManager()->AddObserver(this);
  OnPersonalDataChanged();

  if (!first_inflation) {
    return;
  }

  for (const auto& element_check :
       proto_.show_generic_ui().periodic_element_checks().element_checks()) {
    preconditions_.emplace_back(std::make_unique<ElementPrecondition>(
        element_check.element_condition()));
  }
  if (proto_.show_generic_ui().allow_interrupt() ||
      std::any_of(
          preconditions_.begin(), preconditions_.end(),
          [&](const auto& precondition) { return !precondition->empty(); })) {
    has_pending_wait_for_dom_ = true;

    delegate_->WaitForDom(
        base::TimeDelta::Max(), proto_.show_generic_ui().allow_interrupt(),
        this,
        base::BindRepeating(&ShowGenericUiAction::RegisterChecks,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ShowGenericUiAction::OnWaitForElementTimed,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::BindOnce(&ShowGenericUiAction::OnDoneWaitForDom,
                                      weak_ptr_factory_.GetWeakPtr())));
  }
  wait_time_start_ = base::TimeTicks::Now();
}

void ShowGenericUiAction::OnNavigationEnded() {
  action_stopwatch_.TransferToWaitTime(base::TimeTicks::Now() -
                                       wait_time_start_);
  processed_action_proto_->mutable_show_generic_ui_result()
      ->set_navigation_ended(true);
  OnEndActionInteraction(ClientStatus(ACTION_APPLIED));
}

void ShowGenericUiAction::RegisterChecks(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  if (!callback_) {
    // Action is done; checks aren't necessary anymore.
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  for (size_t i = 0; i < preconditions_.size(); i++) {
    preconditions_[i]->Check(
        checker, base::BindOnce(&ShowGenericUiAction::OnPreconditionResult,
                                weak_ptr_factory_.GetWeakPtr(), i));
  }
  // Let WaitForDom know we're still waiting for elements.
  checker->AddAllDoneCallback(base::BindOnce(
      &ShowGenericUiAction::OnElementChecksDone, weak_ptr_factory_.GetWeakPtr(),
      std::move(wait_for_dom_callback)));
}

void ShowGenericUiAction::OnPreconditionResult(
    size_t precondition_index,
    const ClientStatus& status,
    const std::vector<std::string>& ignored_payloads,
    const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements) {
  if (should_end_action_) {
    return;
  }
  delegate_->GetUserModel()->SetValue(proto_.show_generic_ui()
                                          .periodic_element_checks()
                                          .element_checks(precondition_index)
                                          .model_identifier(),
                                      SimpleValue(status.ok()));
}

void ShowGenericUiAction::OnElementChecksDone(
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  // Calling wait_for_dom_callback with successful status is a way of asking the
  // WaitForDom to end gracefully and call OnDoneWaitForDom with the status.
  // Note that it is possible for WaitForDom to decide not to call
  // OnDoneWaitForDom, if an interrupt triggers at the same time, so we cannot
  // cancel the prompt and choose the suggestion just yet.
  if (should_end_action_) {
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }
  // Let WaitForDom know we're still waiting for an element.
  std::move(wait_for_dom_callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED));
}

void ShowGenericUiAction::OnDoneWaitForDom(const ClientStatus& status) {
  if (!callback_) {
    return;
  }
  EndAction(status);
}

void ShowGenericUiAction::OnEndActionInteraction(const ClientStatus& status) {
  // If WaitForDom was called, we end the action the next time the callback
  // is called in order to end WaitForDom gracefully.
  if (has_pending_wait_for_dom_) {
    should_end_action_ = true;
    return;
  }
  action_stopwatch_.TransferToWaitTime(base::TimeTicks::Now() -
                                       wait_time_start_);
  EndAction(status);
}

void ShowGenericUiAction::EndAction(const ClientStatus& status) {
  if (!callback_) {
    // Avoid race condition: it is possible that a breaking navigation event
    // occurs immediately before or after the action would end naturally.
    return;
  }

  delegate_->ClearGenericUi();
  delegate_->CleanUpAfterPrompt();
  UpdateProcessedAction(status);
  if (status.ok()) {
    const auto& output_model_identifiers =
        proto_.show_generic_ui().output_model_identifiers();
    auto values =
        delegate_->GetUserModel()->GetValues(output_model_identifiers);
    // This should always be the case since there is no way to erase a value
    // from the model.
    DCHECK(values.has_value());
    auto* output_model =
        processed_action_proto_->mutable_show_generic_ui_result()
            ->mutable_model();
    for (size_t i = 0; i < values->size(); ++i) {
      auto* output_value = output_model->add_values();
      output_value->set_identifier(output_model_identifiers.at(i));
      if (!values->at(i).is_client_side_only()) {
        *output_value->mutable_value() = values->at(i);
      }
    }
  }

  std::move(callback_).Run(std::move(processed_action_proto_));
}

void ShowGenericUiAction::OnPersonalDataChanged() {
  if (proto_.show_generic_ui().has_request_profiles()) {
    auto profiles = std::make_unique<
        std::vector<std::unique_ptr<autofill::AutofillProfile>>>();
    for (const auto* profile :
         delegate_->GetPersonalDataManager()->GetProfilesToSuggest()) {
      profiles->emplace_back(MakeUniqueFromProfile(*profile));
    }
    WriteProfilesToUserModel(std::move(profiles),
                             proto_.show_generic_ui().request_profiles(),
                             delegate_->GetUserModel());
  }

  if (proto_.show_generic_ui().has_request_credit_cards()) {
    auto credit_cards =
        std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
    for (const auto* credit_card :
         delegate_->GetPersonalDataManager()->GetCreditCardsToSuggest(true)) {
      credit_cards->emplace_back(
          std::make_unique<autofill::CreditCard>(*credit_card));
    }
    WriteCreditCardsToUserModel(std::move(credit_cards),
                                proto_.show_generic_ui().request_credit_cards(),
                                delegate_->GetUserModel());
  }
}

}  // namespace autofill_assistant
