// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill::payments {

PaymentsFormDataImporter::PaymentsFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)),
      credit_card_save_manager_(std::make_unique<CreditCardSaveManager>(client))
#if !BUILDFLAG(IS_IOS)
      ,
      iban_save_manager_(std::make_unique<IbanSaveManager>(client))
#endif  // !BUILDFLAG(IS_IOS)
{
}

PaymentsFormDataImporter::~PaymentsFormDataImporter() = default;

std::optional<Iban> PaymentsFormDataImporter::ExtractIban(
    const FormStructure& form) {
  Iban candidate_iban = ExtractIbanFromForm(form);
  if (candidate_iban.value().empty()) {
    return std::nullopt;
  }

  // Sets the `kAutofillHasSeenIban` pref to `true` indicating that the user has
  // submitted a form with an IBAN, which indicates that the user is familiar
  // with IBANs as a concept. We set the pref so that even if the user travels
  // to a country where IBAN functionality is not typically used, they will
  // still be able to save new IBANs from the settings page using this pref.
  payments_data_manager().SetAutofillHasSeenIban();

  return candidate_iban;
}

void PaymentsFormDataImporter::CacheFetchedVirtualCard(
    const std::u16string& last_four) {
  fetched_virtual_cards_.insert(last_four);
}

bool PaymentsFormDataImporter::ProcessIbanImportCandidate(
    Iban& extracted_iban) {
  // If a flow where there was no interactive authentication was completed,
  // re-auth opt-in flow might be offered.
  if (auto* mandatory_reauth_manager =
          client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager();
      mandatory_reauth_manager &&
      mandatory_reauth_manager->ShouldOfferOptin(
          payment_method_type_if_non_interactive_authentication_flow_completed_)) {
    payment_method_type_if_non_interactive_authentication_flow_completed_
        .reset();
    mandatory_reauth_manager->StartOptInFlow();
    return true;
  }

  return iban_save_manager_->AttemptToOfferSave(extracted_iban);
}

PaymentsFormDataImporter::ExtractCreditCardFromFormResult
PaymentsFormDataImporter::ExtractCreditCardFromForm(const FormStructure& form) {
  // Populated by the lambdas below.
  ExtractCreditCardFromFormResult result;
  std::string app_locale = client_->GetAppLocale();

  // Populates `result` from `field` if it's a credit card field.
  // For example, if `field` contains credit card number, this sets the number
  // of `result.card` to the `field`'s value.
  auto extract_if_credit_card_field = [&result,
                                       app_locale](const AutofillField& field) {
    std::u16string value = [&field] {
      if (field.Type().GetCreditCardType() == FieldType::CREDIT_CARD_NUMBER) {
        // Credit card numbers are sometimes obfuscated on form submission.
        // Therefore, we give preference to the user input over the field value.
        std::u16string user_input = field.user_input();
        base::TrimWhitespace(user_input, base::TRIM_ALL);
        if (!user_input.empty()) {
          return user_input;
        }
      }
      return field.value_for_import();
    }();
    base::TrimWhitespace(value, base::TRIM_ALL);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (value.empty() ||
        !field.Type().GetGroups().contains(FieldTypeGroup::kCreditCard)) {
      return;
    }
    std::u16string old_value = result.card.GetInfo(field.Type(), app_locale);
    if (field.form_control_type() == FormControlType::kInputMonth) {
      // If |field| is an HTML5 month input, handle it as a special case.
      DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                field.Type().GetCreditCardType());
      result.card.SetInfoForMonthInputType(value);
    } else {
      // If the credit card number offset is within the range of the old value,
      // replace the portion of the old value with the value from the current
      // field. For example:
      // old value: '1234', offset: 4, new value:'5678', result: '12345678'
      // old value: '12345678', offset: 4, new value:'0000', result: '12340000'
      if (field.credit_card_number_offset() > 0 &&
          field.credit_card_number_offset() <= old_value.size()) {
        value = old_value.replace(field.credit_card_number_offset(),
                                  value.size(), value);
      }
      bool saved = result.card.SetInfo(field.Type(), value, app_locale);
      if (!saved && field.IsSelectElement()) {
        // Saving with the option text (here `value`) may fail for the
        // expiration month. Attempt to save with the option value. First find
        // the index of the option text in the select options and try the
        // corresponding value.
        if (auto it =
                std::ranges::find(field.options(), value, &SelectOption::text);
            it != field.options().end()) {
          result.card.SetInfo(field.Type(), it->value, app_locale);
        }
      }
    }

    std::u16string new_value = result.card.GetInfo(field.Type(), app_locale);
    // Skip duplicate field check if the field is a split credit card
    // number field.
    const bool skip_duplication_check =
        field.Type().GetCreditCardType() == FieldType::CREDIT_CARD_NUMBER &&
        field.credit_card_number_offset() > 0;
    result.has_duplicate_credit_card_field_type |=
        !skip_duplication_check && !old_value.empty() && old_value != new_value;
  };

  // Populates `result` from `fields` that satisfy `pred`, and erases those
  // fields. Afterwards, it also erases all remaining fields whose type is now
  // present in `result.card`.
  // For example, if a `CREDIT_CARD_NAME_FULL` field matches `pred`, this
  // function sets the credit card first, last, and full name and erases
  // all `fields` of type `CREDIT_CARD_NAME_{FULL,FIRST,LAST}`.
  auto extract_data_and_remove_field_if =
      [&result, &extract_if_credit_card_field, &app_locale](
          std::vector<const AutofillField*>& fields, const auto& pred) {
        for (const AutofillField* field : fields) {
          if (std::invoke(pred, *field)) {
            extract_if_credit_card_field(*field);
          }
        }
        std::erase_if(fields, [&](const AutofillField* field) {
          return std::invoke(pred, *field) ||
                 !result.card.GetInfo(field->Type(), app_locale).empty();
        });
      };

  // We split the fields into three priority groups: user-typed values,
  // autofilled values, other values. The duplicate-value recognition is limited
  // to values of the respective group.
  //
  // Suppose the user first autofills a form, including invisible fields. Then
  // they edited a visible fields. The priority groups ensure that the invisible
  // field does not prevent credit card import.
  std::vector<const AutofillField*> fields;
  fields.reserve(form.fields().size());
  for (const std::unique_ptr<AutofillField>& field : form.fields()) {
    fields.push_back(field.get());
  }
  extract_data_and_remove_field_if(fields, [](const auto& field) {
    return field.all_modifiers().contains(FieldModifier::kUser);
  });
  extract_data_and_remove_field_if(fields, [](const AutofillField& field) {
    return field.last_modifier() == FieldModifier::kAutofill;
  });
  extract_data_and_remove_field_if(fields, [](const auto&) { return true; });
  return result;
}

bool PaymentsFormDataImporter::ShouldProcessExtractedCreditCard() {
  // Processing should not occur if the current window is a tab modal pop-up, as
  // no credit card save or feature enrollment should happen in this case.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSkipSaveCardForTabModalPopup) &&
      client_->GetPaymentsAutofillClient()->IsTabModalPopupDeprecated()) {
    return false;
  }

  // If there is no `credit_card_import_type_` from form extraction, the
  // extracted card is not a viable candidate for processing.
  if (credit_card_import_type_ ==
      PaymentsFormDataImporter::CreditCardImportType::kNoCard) {
    return false;
  }

  return true;
}

void PaymentsFormDataImporter::
    SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
        std::optional<NonInteractivePaymentMethodType>
            payment_method_type_if_non_interactive_authentication_flow_completed) {
  payment_method_type_if_non_interactive_authentication_flow_completed_ =
      payment_method_type_if_non_interactive_authentication_flow_completed;
}

std::optional<CreditCard> PaymentsFormDataImporter::ExtractCreditCard(
    const FormStructure& form) {
  // The candidate for credit card import. There are many ways for the candidate
  // to be rejected as indicated by the `return std::nullopt` statements below.
  auto [candidate, form_has_duplicate_cc_type] =
      ExtractCreditCardFromForm(form);
  if (form_has_duplicate_cc_type) {
    return std::nullopt;
  }

  if (candidate.IsValid()) {
    AutofillMetrics::LogSubmittedCardStateMetric(
        AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE);
  } else {
    if (candidate.HasValidCardNumber()) {
      AutofillMetrics::LogSubmittedCardStateMetric(
          AutofillMetrics::HAS_CARD_NUMBER_ONLY);
    }
    if (candidate.HasValidExpirationDate()) {
      AutofillMetrics::LogSubmittedCardStateMetric(
          AutofillMetrics::HAS_EXPIRATION_DATE_ONLY);
    }
  }

  // Cards with invalid expiration dates can be uploaded due to the existence of
  // the expiration date fix flow. However, cards with invalid card numbers must
  // still be ignored.
  if (!candidate.HasValidCardNumber()) {
    return std::nullopt;
  }

  // If Save and Fill suggestion was clicked (regardless of whether the card was
  // saved or not eventually) before the form extraction, don't offer other
  // payments post-checkout flows.
  if (fetched_payments_data_context().card_submitted_through_save_and_fill) {
    return std::nullopt;
  }

  // If the extracted card is a known virtual card, return the extracted card.
  if (fetched_virtual_cards_.contains(candidate.LastFourDigits())) {
    credit_card_import_type_ = CreditCardImportType::kVirtualCard;
    return candidate;
  }

  // Can import one valid card per form. Start by treating it as kNewCard, but
  // overwrite this type if we discover it is already a local or server card.
  credit_card_import_type_ = CreditCardImportType::kNewCard;

  // Attempt to merge with an existing local credit card without presenting a
  // prompt.
  for (const CreditCard* local_card :
       payments_data_manager().GetLocalCreditCards()) {
    // Make a local copy so that the data in `local_credit_cards_` isn't
    // modified directly by the UpdateFromImportedCard() call.
    CreditCard maybe_updated_card = *local_card;
    if (maybe_updated_card.UpdateFromImportedCard(candidate,
                                                  client_->GetAppLocale())) {
      payments_data_manager().UpdateCreditCard(maybe_updated_card);
      credit_card_import_type_ = CreditCardImportType::kLocalCard;
      // Update `candidate` to reflect all the details of the updated card.
      // `UpdateFromImportedCard` has updated all values except for the
      // extracted CVC, as we will not update that until later after prompting
      // the user to store their CVC.
      std::u16string extracted_cvc = candidate.cvc();
      candidate = maybe_updated_card;
      candidate.set_cvc(extracted_cvc);
    }
  }

  // Return `candidate` if no server card is matched but the card in the form is
  // a valid card.
  return client_->GetFormDataImporter()->TryMatchingExistingServerCard(
      candidate);
}

bool PaymentsFormDataImporter::ProcessExtractedCreditCard(
    const FormStructure& submitted_form,
    const std::optional<CreditCard>& extracted_credit_card,
    bool is_credit_card_upstream_enabled,
    ukm::SourceId ukm_source_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillPrioritizeSaveCardOverMandatoryReauth) &&
      ProceedWithCardMandatoryReauthOptInIfApplicable()) {
    return true;
  }

  // All of following processing requires the extracted credit card to exist.
  if (!extracted_credit_card.has_value()) {
    return false;
  }

  // If a virtual card was extracted from the form, we do not do anything with
  // virtual cards beyond this point. If
  // `kAutofillPrioritizeSaveCardOverMandatoryReauth` is enabled, try to offer
  // mandatory re-auth before returning.
  if (credit_card_import_type_ ==
      PaymentsFormDataImporter::CreditCardImportType::kVirtualCard) {
    return base::FeatureList::IsEnabled(
               features::kAutofillPrioritizeSaveCardOverMandatoryReauth) &&
           ProceedWithCardMandatoryReauthOptInIfApplicable();
  }

  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return false;
  }

  auto* virtual_card_enrollment_manager =
      client_->GetPaymentsAutofillClient()->GetVirtualCardEnrollmentManager();
  auto& context = fetched_payments_data_context();
  if (virtual_card_enrollment_manager &&
      virtual_card_enrollment_manager->ShouldOfferVirtualCardEnrollment(
          *extracted_credit_card, context.fetched_card_instrument_id,
          context.card_was_fetched_from_cache)) {
    virtual_card_enrollment_manager->InitVirtualCardEnroll(
        *extracted_credit_card, VirtualCardEnrollmentSource::kDownstream,
        base::BindOnce(
            &VirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble,
            base::Unretained(virtual_card_enrollment_manager)));
    return true;
  }

  // Proceed with card or CVC saving if applicable.
  if (credit_card_save_manager_->ProceedWithSavingIfApplicable(
          submitted_form, *extracted_credit_card, credit_card_import_type_,
          is_credit_card_upstream_enabled, ukm_source_id)) {
    return true;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillPrioritizeSaveCardOverMandatoryReauth) &&
      ProceedWithCardMandatoryReauthOptInIfApplicable()) {
    // Try to offer mandatory re-auth as the last step.
    return true;
  }

  return false;
}

bool PaymentsFormDataImporter::
    ProceedWithCardMandatoryReauthOptInIfApplicable() {
  // If a flow without interactive authentication was completed and the user
  // didn't update the result that was filled into the form, re-auth opt-in
  // flow might be offered.
  if (auto* mandatory_reauth_manager =
          client_->GetPaymentsAutofillClient()
              ->GetOrCreatePaymentsMandatoryReauthManager();
      credit_card_import_type_ != CreditCardImportType::kNewCard &&
      mandatory_reauth_manager &&
      mandatory_reauth_manager->ShouldOfferOptin(
          payment_method_type_if_non_interactive_authentication_flow_completed_)) {
    payment_method_type_if_non_interactive_authentication_flow_completed_
        .reset();
    mandatory_reauth_manager->StartOptInFlow();
    return true;
  }
  return false;
}

Iban PaymentsFormDataImporter::ExtractIbanFromForm(const FormStructure& form) {
  // Creates an IBAN candidate with `kUnknown` record type as it is currently
  // unknown if this IBAN already exists locally or on the server.
  Iban candidate_iban;
  for (const auto& field : form) {
    const std::u16string& value = field->value_for_import();
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    if (field->Type().GetTypes().contains(IBAN_VALUE) && Iban::IsValid(value)) {
      candidate_iban.set_value(value);
      break;
    }
  }
  return candidate_iban;
}

PaymentsDataManager& PaymentsFormDataImporter::payments_data_manager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill::payments
