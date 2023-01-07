// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/external_action.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {
namespace {

void SetInfoOnAutofillDataModel(
    const google::protobuf::Map<google::protobuf::int32, std::string>&
        type_to_value,
    const google::protobuf::Map<google::protobuf::int32,
                                google::protobuf::int32>& type_to_status,
    autofill::AutofillDataModel* model) {
  for (const auto& [type, value] : type_to_value) {
    DCHECK(type_to_status.contains(type));
    model->SetRawInfoWithVerificationStatus(
        static_cast<autofill::ServerFieldType>(type), base::UTF8ToUTF16(value),
        static_cast<autofill::structured_address::VerificationStatus>(
            type_to_status.at(type)));
  }
}

}  // namespace

ExternalAction::ExternalAction(ActionDelegate* delegate,
                               const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_external_action());
}

ExternalAction::~ExternalAction() = default;

void ExternalAction::InternalProcessAction(ProcessActionCallback callback) {
  callback_ = std::move(callback);
  if (!delegate_->SupportsExternalActions()) {
    VLOG(1) << "External action are not supported for this run.";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }
  if (!proto_.external_action().has_info()) {
    VLOG(1) << "The ExternalAction's |info| is missing.";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  delegate_->RequestExternalAction(
      proto_.external_action(),
      base::BindOnce(&ExternalAction::StartDomChecks,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ExternalAction::OnExternalActionFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  // Do not add any code here. External delegates may choose to end the action
  // immediately, which could result in *this being deleted and UaF errors for
  // code after the above call.
}

void ExternalAction::StartDomChecks(
    ExternalActionDelegate::DomUpdateCallback dom_update_callback) {
  const auto& external_action = proto_.external_action();
  if (!external_action.conditions().empty() ||
      external_action.allow_interrupt()) {
    // We keep track of the fact that we have an active WaitForDom to make sure
    // we end it gracefully.
    has_pending_wait_for_dom_ = true;
    dom_update_callback_ = std::move(dom_update_callback);
    SetupConditions();
    // TODO(b/201964908): fix time tracking.
    delegate_->WaitForDom(
        /* max_wait_time= */ base::TimeDelta::Max(),
        /* allow_observer_mode = */ false, external_action.allow_interrupt(),
        /* observer= */ nullptr,
        base::BindRepeating(&ExternalAction::RegisterChecks,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ExternalAction::OnWaitForElementTimed,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::BindOnce(&ExternalAction::OnDoneWaitForDom,
                                      weak_ptr_factory_.GetWeakPtr())));
  }
}

void ExternalAction::SetupConditions() {
  for (const auto& condition_proto : proto_.external_action().conditions()) {
    ConditionStatus condition_status;
    condition_status.proto = condition_proto;
    conditions_.emplace_back(condition_status);
  }
}

void ExternalAction::RegisterChecks(
    BatchElementChecker* checker,
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  if (!callback_) {
    // Action is done; checks aren't necessary anymore.
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  for (size_t i = 0; i < conditions_.size(); i++) {
    checker->AddElementConditionCheck(
        conditions_[i].proto.element_condition(),
        base::BindOnce(&ExternalAction::OnPreconditionResult,
                       weak_ptr_factory_.GetWeakPtr(), i));
  }

  checker->AddAllDoneCallback(base::BindOnce(
      &ExternalAction::OnElementChecksDone, weak_ptr_factory_.GetWeakPtr(),
      std::move(wait_for_dom_callback)));
}

void ExternalAction::OnPreconditionResult(
    size_t condition_index,
    const ClientStatus& status,
    const std::vector<std::string>& ignored_payloads,
    const std::vector<std::string>& ignored_tags,
    const base::flat_map<std::string, DomObjectFrameStack>& ignored_elements) {
  DCHECK_LT(condition_index, conditions_.size());
  bool precondition_is_met = status.ok();

  // If this is the first time we perform the check, we consider the
  // precondition as 'changed' since we always want to send the notification
  // after the first check.
  if (first_condition_notification_sent_ &&
      conditions_[condition_index].result == precondition_is_met) {
    return;
  }

  conditions_[condition_index].result = precondition_is_met;
  conditions_[condition_index].changed = true;
}

void ExternalAction::OnElementChecksDone(
    base::OnceCallback<void(const ClientStatus&)> wait_for_dom_callback) {
  // If it was decided to end the action in the meantime, we send an OK status
  // to WaitForDom so that it can end gracefully. Note that it is possible for
  // WaitForDom to decide not to call OnDoneWaitForDom, if an interrupt triggers
  // at the same time.
  if (external_action_end_requested_) {
    std::move(wait_for_dom_callback).Run(OkClientStatus());
    return;
  }

  external::ElementConditionsUpdate update_proto;
  for (auto& condition : conditions_) {
    if (!condition.changed)
      continue;

    condition.changed = false;
    external::ElementConditionsUpdate::ConditionResult result;
    result.set_id(condition.proto.id());
    result.set_satisfied(condition.result);
    *update_proto.add_results() = result;
  }

  // We only send the notification if there were any changes since the last
  // check.
  if (!update_proto.results().empty()) {
    first_condition_notification_sent_ = true;
    dom_update_callback_.Run(update_proto);
  }

  // Whether we had satisfied element conditions or not, we run this callback
  // with |ELEMENT_RESOLUTION_FAILED| to let the WaitForDom know that we want to
  // keep running checks.
  std::move(wait_for_dom_callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED));
}

void ExternalAction::OnDoneWaitForDom(const ClientStatus& status) {
  if (!callback_) {
    return;
  }

  // If |status| is OK we send the external response. Otherwise we ignore the
  // external response and report the error, as we might not be in a state to
  // continue with the script.
  if (status.ok() && external_action_end_requested_) {
    ProcessExternalResult();
    return;
  }
  EndAction(status);
}

void ExternalAction::OnExternalActionFinished(const external::Result& result) {
  if (!callback_) {
    return;
  }

  external_action_result_ = result;

  // If there is an ongoing WaitForDom, we end the action on the next WaitForDom
  // notification to make sure we end gracefully.
  if (has_pending_wait_for_dom_) {
    external_action_end_requested_ = true;
    return;
  }

  ProcessExternalResult();
}

void ExternalAction::ProcessExternalResult() {
  if (!external_action_result_.selected_profiles().empty()) {
    SetSelectedProfiles(external_action_result_.selected_profiles());
  }
  if (external_action_result_.has_selected_credit_card()) {
    SetSelectedCreditCard(external_action_result_.selected_credit_card());
  }

  *processed_action_proto_->mutable_external_action_result()
       ->mutable_result_info() = external_action_result_.result_info();
  EndAction(external_action_result_.success()
                ? ClientStatus(ACTION_APPLIED)
                : ClientStatus(UNKNOWN_ACTION_STATUS));
}

void ExternalAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

void ExternalAction::SetSelectedProfiles(
    const google::protobuf::Map<std::string,
                                autofill_assistant::external::ProfileProto>&
        profiles_proto) {
  for (const auto& [profile_name, profile_proto] : profiles_proto) {
    DCHECK(profile_proto.data().has_guid() &&
           profile_proto.data().has_origin());

    auto autofill_profile = std::make_unique<autofill::AutofillProfile>(
        /* guid= */ profile_proto.data().guid(),
        /* origin= */ profile_proto.data().origin());

    SetInfoOnAutofillDataModel(profile_proto.data().values(),
                               profile_proto.data().verification_statuses(),
                               autofill_profile.get());

    autofill_profile->FinalizeAfterImport();

    delegate_->GetUserModel()->SetSelectedAutofillProfile(
        profile_name, std::move(autofill_profile),
        delegate_->GetMutableUserData());
  }
}

void ExternalAction::SetSelectedCreditCard(
    const autofill_assistant::external::CreditCardProto& credit_card_proto) {
  DCHECK(credit_card_proto.data().has_guid() &&
         credit_card_proto.data().has_origin());

  auto credit_card = std::make_unique<autofill::CreditCard>(
      /* guid= */ credit_card_proto.data().guid(),
      /* origin= */ credit_card_proto.data().origin());

  SetInfoOnAutofillDataModel(credit_card_proto.data().values(),
                             credit_card_proto.data().verification_statuses(),
                             credit_card.get());

  if (credit_card_proto.has_record_type()) {
    credit_card->set_record_type(static_cast<autofill::CreditCard::RecordType>(
        credit_card_proto.record_type()));
  }

  if (credit_card_proto.has_instrument_id()) {
    credit_card->set_instrument_id(credit_card_proto.instrument_id());
  }

  if (!credit_card_proto.network().empty() &&
      credit_card->record_type() ==
          autofill::CreditCard::RecordType::MASKED_SERVER_CARD) {
    credit_card->SetNetworkForMaskedCard(credit_card_proto.network());
  }

  if (!credit_card_proto.server_id().empty()) {
    credit_card->set_server_id(credit_card_proto.server_id());
  }

  delegate_->GetUserModel()->SetSelectedCreditCard(
      std::move(credit_card), delegate_->GetMutableUserData());
}

}  // namespace autofill_assistant
