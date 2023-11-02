// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/element.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

void ShowGenericUiAction::OnInterruptStarted() {
  delegate_->ClearGenericUi();
}

void ShowGenericUiAction::OnInterruptFinished() {
  delegate_->SetGenericUi(
      std::make_unique<GenericUserInterfaceProto>(
          proto_.show_generic_ui().generic_user_interface()),
      base::BindOnce(&ShowGenericUiAction::OnEndActionInteraction,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ShowGenericUiAction::OnViewInflationFinished,
                     weak_ptr_factory_.GetWeakPtr(), false),
      base::BindRepeating(&ShowGenericUiAction::OnRequestBackendUserData,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ShowGenericUiAction::OnShowAccountScreen,
                          weak_ptr_factory_.GetWeakPtr())

  );
}

ShowGenericUiAction::ShowGenericUiAction(ActionDelegate* delegate,
                                         const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_show_generic_ui());
}

ShowGenericUiAction::~ShowGenericUiAction() = default;

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
    if (!delegate_->GetUserData()->HasAdditionalValue(
            additional_value.source_identifier())) {
      EndAction(ClientStatus(PRECONDITION_FAILED));
      return;
    }
  }
  for (const auto& additional_value :
       proto_.show_generic_ui().request_user_data().additional_values()) {
    ValueProto value = *delegate_->GetUserData()->GetAdditionalValue(
        additional_value.source_identifier());
    value.set_is_client_side_only(true);
    delegate_->GetUserModel()->SetValue(additional_value.model_identifier(),
                                        value);
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
                     /* first_inflation= */ true),
      base::BindRepeating(&ShowGenericUiAction::OnRequestBackendUserData,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ShowGenericUiAction::OnShowAccountScreen,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ShowGenericUiAction::OnShowAccountScreen(
    const ShowAccountScreenProto& proto) {
  if (!proto.has_gms_account_intent_screen_id()) {
    LOG(ERROR) << "Screen information not present in ShowAccountScreenProto";
    return;
  }
  delegate_->ShowAccountScreen(
      proto, delegate_->GetEmailAddressForAccessTokenAccount());
}

void ShowGenericUiAction::OnRequestBackendUserData(
    const RequestBackendDataProto& request) {
  if (request.output_success_model_identifier().empty() ||
      request.request_phone_numbers()
          .output_profiles_model_identifier()
          .empty()) {
    return;
  }
  // TODO(b/246875491): Stop using collect user data options in ShowGenericUi.
  CollectUserDataOptions options;
  options.request_phone_number_separately = request.has_request_phone_numbers();
  delegate_->RequestUserData(
      options, base::BindOnce(&ShowGenericUiAction::OnGetBackendUserData,
                              weak_ptr_factory_.GetWeakPtr(), request));
}

void ShowGenericUiAction::OnGetBackendUserData(
    const RequestBackendDataProto& request,
    bool success,
    const GetUserDataResponseProto& response) {
  delegate_->GetUserModel()->SetValue(request.output_success_model_identifier(),
                                      SimpleValue(success));
  if (request.has_request_phone_numbers()) {
    auto phone_number_autofill_profile_list = std::make_unique<
        std::vector<std::unique_ptr<autofill::AutofillProfile>>>();
    for (const auto& phone_number_proto : response.available_phone_numbers()) {
      auto profile = std::make_unique<autofill::AutofillProfile>();
      // AddAutofillEntryToDataModel adds only to the autofill profile and
      // not UserModel.
      user_data::AddAutofillEntryToDataModel(
          autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
          phone_number_proto.value(), response.locale(), profile.get());
      phone_number_autofill_profile_list->emplace_back(std::move(profile));
    }
    size_t phone_numbers_count = phone_number_autofill_profile_list->size();
    delegate_->GetUserModel()->SetPhoneNumbers(
        std::move(phone_number_autofill_profile_list));

    auto phone_number_model_list_value = ValueProto();
    auto* phone_number_model_list =
        phone_number_model_list_value.mutable_profiles();
    for (size_t i = 0; i < phone_numbers_count; i++) {
      phone_number_model_list->add_values()->set_phone_number_index(i);
    }
    phone_number_model_list_value.set_is_client_side_only(true);
    delegate_->GetUserModel()->SetValue(
        request.request_phone_numbers().output_profiles_model_identifier(),
        phone_number_model_list_value);
  }
}

void ShowGenericUiAction::OnViewInflationFinished(bool first_inflation,
                                                  const ClientStatus& status) {
  if (!status.ok()) {
    EndAction(status);
    return;
  }

  if (!first_inflation) {
    return;
  }

  for (const auto& element_check :
       proto_.show_generic_ui().periodic_element_checks().element_checks()) {
    preconditions_.emplace_back(element_check.element_condition());
  }
  if (proto_.show_generic_ui().allow_interrupt() ||
      base::ranges::any_of(preconditions_, [&](const auto& precondition) {
        return !BatchElementChecker::IsElementConditionEmpty(precondition);
      })) {
    has_pending_wait_for_dom_ = true;

    // TODO(b/219004758): Enable observer-based WaitForDom, which would require
    // negating the preconditions that matched in the previous check, so that we
    // are alerted every time one changes instead of every time one becomes
    // true.
    delegate_->WaitForDom(
        /* max_wait_time= */ base::TimeDelta::Max(),
        /* allow_observer_mode = */ false,
        proto_.show_generic_ui().allow_interrupt(),
        /* observer= */ this,
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
    checker->AddElementConditionCheck(
        preconditions_[i],
        base::BindOnce(&ShowGenericUiAction::OnPreconditionResult,
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
    const std::vector<std::string>& ignored_tags,
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

}  // namespace autofill_assistant
