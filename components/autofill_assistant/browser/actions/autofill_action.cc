// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/autofill_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "content/public/browser/web_contents.h"

namespace autofill_assistant {

namespace {
// Self-deleting requester of full card details, including full PAN and the CVC
// number.
class SelfDeleteFullCardRequester
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  SelfDeleteFullCardRequester() : weak_ptr_factory_(this) {}

  using GetFullCardCallback =
      base::OnceCallback<void(std::unique_ptr<autofill::CreditCard>,
                              const base::string16& cvc)>;
  void GetFullCard(content::WebContents* web_contents,
                   autofill::CreditCard* card,
                   GetFullCardCallback callback) {
    DCHECK(card);
    callback_ = std::move(callback);

    autofill::ContentAutofillDriverFactory* factory =
        autofill::ContentAutofillDriverFactory::FromWebContents(web_contents);
    if (!factory) {
      OnFullCardRequestFailed();
      return;
    }

    autofill::ContentAutofillDriver* driver =
        factory->DriverForFrame(web_contents->GetMainFrame());
    if (!driver) {
      OnFullCardRequestFailed();
      return;
    }

    driver->autofill_manager()->GetOrCreateFullCardRequest()->GetFullCard(
        *card, autofill::AutofillClient::UNMASK_FOR_AUTOFILL,
        weak_ptr_factory_.GetWeakPtr(),
        driver->autofill_manager()->GetAsFullCardRequestUIDelegate());
  }

 private:
  ~SelfDeleteFullCardRequester() override {}

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& /* full_card_request */,
      const autofill::CreditCard& card,
      const base::string16& cvc) override {
    std::move(callback_).Run(std::make_unique<autofill::CreditCard>(card), cvc);
    delete this;
  }

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestFailed() override {
    // Failed might because of cancel, so return nullptr to notice caller.
    std::move(callback_).Run(nullptr, base::string16());
    delete this;
  }

  GetFullCardCallback callback_;

  base::WeakPtrFactory<SelfDeleteFullCardRequester> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(SelfDeleteFullCardRequester);
};

}  // namespace

AutofillAction::AutofillAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  if (proto.has_use_address()) {
    is_autofill_card_ = false;
    prompt_ = proto.use_address().prompt();
    name_ = proto.use_address().name();
    selectors_ =
        ExtractSelectors(proto.use_address().form_field_element().selectors());
    fill_form_message_ = proto.use_address().strings().fill_form();
    check_form_message_ = proto.use_address().strings().check_form();
    required_fields_value_status_.resize(
        proto_.use_address().required_fields_size(), UNKNOWN);
    show_overlay_ = proto.use_address().show_overlay();
  } else {
    DCHECK(proto.has_use_card());
    is_autofill_card_ = true;
    prompt_ = proto.use_card().prompt();
    name_ = "";
    selectors_ =
        ExtractSelectors(proto.use_card().form_field_element().selectors());
    fill_form_message_ = proto.use_card().strings().fill_form();
    check_form_message_ = proto.use_card().strings().check_form();
    show_overlay_ = proto.use_card().show_overlay();
  }
}

AutofillAction::~AutofillAction() = default;

void AutofillAction::InternalProcessAction(
    ActionDelegate* delegate,
    ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);
  // Check data already selected in a previous action.
  base::Optional<std::string> selected_data;
  if (is_autofill_card_) {
    selected_data = delegate->GetClientMemory()->selected_card();
  } else {
    selected_data = delegate->GetClientMemory()->selected_address(name_);
  }

  if (selected_data) {
    const std::string& guid = selected_data.value();
    if (guid.empty()) {
      // User selected 'Fill manually'.
      delegate->StopCurrentScript(fill_form_message_);
      EndAction(/* successful= */ true);
      return;
    }

    if (selectors_.empty()) {
      // If there is no selector, finish the action directly.
      EndAction(/* successful= */ true);
      return;
    }

    FillFormWithData(guid, delegate);
    return;
  }

  // Show prompt.
  if (!prompt_.empty()) {
    delegate->ShowStatusMessage(prompt_);
  }

  // Ask user to select the data.
  base::OnceCallback<void(const std::string&)> selection_callback =
      base::BindOnce(&AutofillAction::OnDataSelected,
                     weak_ptr_factory_.GetWeakPtr(), delegate);
  if (is_autofill_card_) {
    delegate->ChooseCard(std::move(selection_callback));
    return;
  }

  delegate->ChooseAddress(std::move(selection_callback));
}

void AutofillAction::EndAction(bool successful) {
  UpdateProcessedAction(successful ? ACTION_APPLIED : OTHER_ACTION_STATUS);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void AutofillAction::OnDataSelected(ActionDelegate* delegate,
                                    const std::string& guid) {
  // Remember the selection.
  if (is_autofill_card_) {
    delegate->GetClientMemory()->set_selected_card(guid);
  } else {
    delegate->GetClientMemory()->set_selected_address(name_, guid);
  }

  if (selectors_.empty()) {
    // If there is no selector, finish the action directly. This can be the case
    // when we want to trigger the selection of address or card at the beginning
    // of the script and use it later.
    EndAction(/* successful= */ true);
    return;
  }

  if (guid.empty()) {
    delegate->StopCurrentScript(fill_form_message_);
    EndAction(/* successful= */ true);
    return;
  }

  FillFormWithData(guid, delegate);
}

void AutofillAction::FillFormWithData(const std::string& guid,
                                      ActionDelegate* delegate) {
  delegate->WaitForElement(selectors_,
                           base::BindOnce(&AutofillAction::OnWaitForElement,
                                          weak_ptr_factory_.GetWeakPtr(), guid,
                                          base::Unretained(delegate)));
}

void AutofillAction::OnWaitForElement(const std::string& guid,
                                      ActionDelegate* delegate,
                                      bool element_found) {
  if (!element_found) {
    EndAction(/* successful= */ false);
    return;
  }

  DCHECK(!selectors_.empty());
  if (is_autofill_card_) {
    autofill::CreditCard* card =
        delegate->GetPersonalDataManager()->GetCreditCardByGUID(guid);
    DCHECK(card);
    // TODO(crbug.com/806868): Consider refactoring SelfDeleteFullCardRequester
    // so as to unit test it.
    (new SelfDeleteFullCardRequester())
        ->GetFullCard(delegate->GetWebContents(), card,
                      base::BindOnce(&AutofillAction::OnGetFullCard,
                                     weak_ptr_factory_.GetWeakPtr(), delegate));
    return;
  }

  delegate->FillAddressForm(
      guid, selectors_,
      base::BindOnce(&AutofillAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), guid, delegate));
}

void AutofillAction::OnGetFullCard(ActionDelegate* delegate,
                                   std::unique_ptr<autofill::CreditCard> card,
                                   const base::string16& cvc) {
  if (!card) {
    // TODO(crbug.com/806868): The failure might because of cancel, then ask to
    // choose a card again.
    EndAction(false);
    return;
  }

  std::string guid = card->guid();
  delegate->FillCardForm(
      std::move(card), cvc, selectors_,
      base::BindOnce(&AutofillAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), guid, delegate));
}

void AutofillAction::OnFormFilled(const std::string& guid,
                                  ActionDelegate* delegate,
                                  bool successful) {
  // In case Autofill failed, we fail the action.
  if (!successful) {
    EndAction(/* successful= */ false);
    return;
  }

  CheckRequiredFields(guid, delegate, /* allow_fallback */ true);
}

void AutofillAction::CheckRequiredFields(const std::string& guid,
                                         ActionDelegate* delegate,
                                         bool allow_fallback) {
  if (is_autofill_card_) {
    // TODO(crbug.com/806868): Implement required fields checking for cards.
    EndAction(/* successful= */ true);
    return;
  }

  // If there are no required fields, finish the action successfully.
  if (proto_.use_address().required_fields().empty()) {
    EndAction(/* successful= */ true);
    return;
  }

  DCHECK(!batch_element_checker_);
  batch_element_checker_ = delegate->CreateBatchElementChecker();
  for (int i = 0; i < proto_.use_address().required_fields_size(); i++) {
    auto& required_address_field = proto_.use_address().required_fields(i);
    DCHECK_GT(required_address_field.element().selectors_size(), 0);
    batch_element_checker_->AddFieldValueCheck(
        ExtractSelectors(required_address_field.element().selectors()),
        base::BindOnce(&AutofillAction::OnGetRequiredFieldValue,
                       // this instance owns batch_element_checker_
                       base::Unretained(this), i));
  }
  batch_element_checker_->Run(
      base::TimeDelta::FromSeconds(0),
      /* try_done= */ base::DoNothing(),
      /* all_done= */
      base::BindOnce(
          &AutofillAction::OnCheckRequiredFieldsDone,
          base::Unretained(this),  // this instance owns batch_element_checker_
          guid, base::Unretained(delegate), allow_fallback));
}

void AutofillAction::OnGetRequiredFieldValue(int required_fields_index,
                                             bool exists,
                                             const std::string& value) {
  required_fields_value_status_[required_fields_index] =
      value.empty() ? EMPTY : NOT_EMPTY;
}

void AutofillAction::OnCheckRequiredFieldsDone(const std::string& guid,
                                               ActionDelegate* delegate,
                                               bool allow_fallback) {
  batch_element_checker_.reset();

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool validation_successful = true;
  for (FieldValueStatus status : required_fields_value_status_) {
    if (status == EMPTY) {
      validation_successful = false;
      break;
    }
  }

  if (validation_successful) {
    EndAction(/* successful= */ true);
    return;
  }

  if (!allow_fallback) {
    // Validation failed and we don't want to try the fallback, so we stop
    // the script.
    delegate->StopCurrentScript(check_form_message_);
    EndAction(/* successful= */ true);
    return;
  }

  // If there are any fallbacks for the empty fields, set them, otherwise fail
  // immediately.
  bool has_fallbacks = false;
  auto* profile = delegate->GetPersonalDataManager()->GetProfileByGUID(guid);
  DCHECK(profile);
  for (int i = 0; i < proto_.use_address().required_fields_size(); i++) {
    if (required_fields_value_status_[i] == EMPTY &&
        !GetAddressFieldValue(
             profile, proto_.use_address().required_fields(i).address_field())
             .empty()) {
      has_fallbacks = true;
      break;
    }
  }
  if (!has_fallbacks) {
    delegate->StopCurrentScript(check_form_message_);
    EndAction(/* successful= */ true);
    return;
  }

  // Set the fallback values and check again.
  SetFallbackFieldValuesSequentially(guid, delegate, 0);
}

void AutofillAction::SetFallbackFieldValuesSequentially(
    const std::string& guid,
    ActionDelegate* delegate,
    int required_fields_index) {
  DCHECK_GE(required_fields_index, 0);

  // Skip non-empty fields.
  const auto& required_fields = proto_.use_address().required_fields();
  while (required_fields_index < required_fields.size() &&
         required_fields_value_status_[required_fields_index] != EMPTY) {
    required_fields_index++;
  }

  // If there are no more fields to set, check the required fields again,
  // but this time we don't want to try the fallback in case of failure.
  if (required_fields_index >= required_fields.size()) {
    DCHECK_EQ(required_fields_index, required_fields.size());

    CheckRequiredFields(guid, delegate, /* allow_fallback */ false);
    return;
  }

  // Set the next field to its fallback value.
  std::string fallback_value = base::UTF16ToUTF8(GetAddressFieldValue(
      delegate->GetPersonalDataManager()->GetProfileByGUID(guid),
      required_fields.Get(required_fields_index).address_field()));
  if (fallback_value.empty()) {
    // If there is no fallback value, we skip this failed field.
    SetFallbackFieldValuesSequentially(guid, delegate, ++required_fields_index);
    return;
  }

  DCHECK_GT(
      required_fields.Get(required_fields_index).element().selectors_size(), 0);
  delegate->SetFieldValue(
      ExtractSelectors(
          required_fields.Get(required_fields_index).element().selectors()),
      fallback_value,
      required_fields.Get(required_fields_index).simulate_key_presses(),
      base::BindOnce(&AutofillAction::OnSetFallbackFieldValue,
                     weak_ptr_factory_.GetWeakPtr(), guid, delegate,
                     required_fields_index));
}

void AutofillAction::OnSetFallbackFieldValue(const std::string& guid,
                                             ActionDelegate* delegate,
                                             int required_fields_index,
                                             bool successful) {
  if (!successful) {
    // Fallback failed: we stop the script without checking the fields.
    delegate->StopCurrentScript(check_form_message_);
    EndAction(/* successful= */ true);
    return;
  }
  SetFallbackFieldValuesSequentially(guid, delegate, ++required_fields_index);
}

base::string16 AutofillAction::GetAddressFieldValue(
    const autofill::AutofillProfile* profile,
    const UseAddressProto::RequiredField::AddressField& address_field) {
  // TODO(crbug.com/806868): Get the actual application locale.
  std::string app_locale = "en-US";
  switch (address_field) {
    case UseAddressProto::RequiredField::FIRST_NAME:
      return profile->GetInfo(autofill::NAME_FIRST, app_locale);
    case UseAddressProto::RequiredField::LAST_NAME:
      return profile->GetInfo(autofill::NAME_LAST, app_locale);
    case UseAddressProto::RequiredField::FULL_NAME:
      return profile->GetInfo(autofill::NAME_FULL, app_locale);
    case UseAddressProto::RequiredField::PHONE_NUMBER:
      return profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, app_locale);
    case UseAddressProto::RequiredField::EMAIL:
      return profile->GetInfo(autofill::EMAIL_ADDRESS, app_locale);
    case UseAddressProto::RequiredField::ORGANIZATION:
      return profile->GetInfo(autofill::COMPANY_NAME, app_locale);
    case UseAddressProto::RequiredField::COUNTRY_CODE:
      return profile->GetInfo(autofill::ADDRESS_HOME_COUNTRY, app_locale);
    case UseAddressProto::RequiredField::REGION:
      return profile->GetInfo(autofill::ADDRESS_HOME_STATE, app_locale);
    case UseAddressProto::RequiredField::STREET_ADDRESS:
      return profile->GetInfo(autofill::ADDRESS_HOME_STREET_ADDRESS,
                              app_locale);
    case UseAddressProto::RequiredField::LOCALITY:
      return profile->GetInfo(autofill::ADDRESS_HOME_CITY, app_locale);
    case UseAddressProto::RequiredField::DEPENDANT_LOCALITY:
      return profile->GetInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
                              app_locale);
    case UseAddressProto::RequiredField::POSTAL_CODE:
      return profile->GetInfo(autofill::ADDRESS_HOME_ZIP, app_locale);
    case UseAddressProto::RequiredField::UNDEFINED:
      NOTREACHED();
      return base::string16();
  }
}
}  // namespace autofill_assistant
