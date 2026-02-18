// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/form_data_importer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_import/addresses/address_form_data_importer.h"
#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/history/core/browser/history_service.h"

namespace autofill {

namespace {

bool ShouldProcessExtractedCreditCard(
    const raw_ref<AutofillClient>& client,
    payments::PaymentsFormDataImporter::CreditCardImportType
        credit_card_import_type) {
  // Processing should not occur if the current window is a tab modal pop-up, as
  // no credit card save or feature enrollment should happen in this case.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSkipSaveCardForTabModalPopup) &&
      client->GetPaymentsAutofillClient()->IsTabModalPopupDeprecated()) {
    return false;
  }

  // If there is no `credit_card_import_type` from form extraction, the
  // extracted card is not a viable candidate for processing.
  if (credit_card_import_type ==
      payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard) {
    return false;
  }

  return true;
}

}  // namespace

FormDataImporter::ExtractedFormData::ExtractedFormData() = default;

FormDataImporter::ExtractedFormData::ExtractedFormData(
    const ExtractedFormData& extracted_form_data) = default;

FormDataImporter::ExtractedFormData&
FormDataImporter::ExtractedFormData::operator=(
    const ExtractedFormData& extracted_form_data) = default;

FormDataImporter::ExtractedFormData::~ExtractedFormData() = default;

FormDataImporter::FormDataImporter(AutofillClient* client,
                                   history::HistoryService* history_service)
    : client_(CHECK_DEREF(client)),
      credit_card_save_manager_(
          std::make_unique<CreditCardSaveManager>(client)),
      address_form_data_importer_(client),
      payments_form_data_importer_(client) {
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

FormDataImporter::~FormDataImporter() = default;

void FormDataImporter::ImportAndProcessFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled,
    ukm::SourceId ukm_source_id) {
  ExtractedFormData extracted_data =
      ExtractFormData(submitted_form, profile_autofill_enabled,
                      payment_methods_autofill_enabled);

  // Create a vector of extracted address profiles.
  // This is used to make preliminarily imported profiles available
  // to the credit card import logic.
  std::vector<AutofillProfile> preliminary_imported_address_profiles;
  for (const auto& candidate : extracted_data.extracted_address_profiles) {
    preliminary_imported_address_profiles.push_back(candidate.profile);
  }
  credit_card_save_manager_->SetPreliminarilyImportedAutofillProfile(
      preliminary_imported_address_profiles);

  bool payments_prompt_potentially_shown = false;
  if (ShouldProcessExtractedCreditCard(client_, credit_card_import_type_)) {
    // Only check IsCreditCardUploadEnabled() if conditions that enable
    // processing of the extracted credit card are true, in order to prevent
    // the metrics it logs from being diluted by cases where extracted credit
    // cards should not be processed or there was no credit card to process.
    bool credit_card_upload_enabled =
        credit_card_save_manager_->IsCreditCardUploadEnabled();
    payments_prompt_potentially_shown = ProcessExtractedCreditCard(
        submitted_form, extracted_data.extracted_credit_card,
        credit_card_upload_enabled, ukm_source_id);
  }

  bool iban_prompt_potentially_shown = false;
  if (extracted_data.extracted_iban.has_value() &&
      payment_methods_autofill_enabled) {
    iban_prompt_potentially_shown =
        payments_form_data_importer_.ProcessIbanImportCandidate(
            *extracted_data.extracted_iban);
  }

  // Reset last fetch payments method metadata after all payments related form
  // data processing logic is finished.
  GetPaymentsFormDataImporter().fetched_payments_data_context() =
      payments::PaymentsFormDataImporter::FetchedPaymentsDataContext();

  // Record the prompt status iff at least one prompt could have been displayed.
  // Recording that status isn't pertinent otherwise. When there is a full
  // profile candidate available for import, it is reasonable to think that
  // either the save or update prompt would have been displayed, which guess is
  // probably not 100% reliable but that's good enough for this metric.
  bool has_full_profile_candidate =
      !preliminary_imported_address_profiles.empty();
  if (has_full_profile_candidate && payments_prompt_potentially_shown) {
    AutofillMetrics::LogAutofillPromptStatus(
        AutofillMetrics::AutofillPromptStatus::kAddressAndCreditCardShown);
  } else if (has_full_profile_candidate) {
    AutofillMetrics::LogAutofillPromptStatus(
        AutofillMetrics::AutofillPromptStatus::kAddressShown);
  } else if (payments_prompt_potentially_shown) {
    AutofillMetrics::LogAutofillPromptStatus(
        AutofillMetrics::AutofillPromptStatus::kCreditCardShown);
  }

  // TODO(crbug.com/356845298) Clean up when launched.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForNameAndEmail)) {
    base::flat_set<std::string> unedited_autofilled_profile_guids =
        GetAddressFormDataImporter().ExtractGUIDsOfProfilesWithoutManualEdits(
            submitted_form);

    for (auto& candidate : extracted_data.extracted_address_profiles) {
      candidate.import_metadata.unedited_autofilled_profile_guids =
          unedited_autofilled_profile_guids;
    }
  }

  GetAddressFormDataImporter().ProcessExtractedAddressProfiles(
      extracted_data.extracted_address_profiles,
      // If a payments prompt is potentially shown, do not allow for a second
      // address profile import dialog.
      /*allow_prompt=*/!payments_prompt_potentially_shown &&
          !iban_prompt_potentially_shown,
      ukm_source_id);
}

FormDataImporter::ExtractedFormData FormDataImporter::ExtractFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled) {
  ExtractedFormData extracted_form_data;
  // We try the same `form` for both credit card and address import/update.
  // - `ExtractCreditCard()` may update an existing card, or fill
  //   `extracted_credit_card` contained in `extracted_form_data` with an
  //   extracted card.
  // - `ExtractAddressProfiles()` collects all importable
  // profiles, but currently
  //   at most one import prompt is shown.
  // Reset `credit_card_import_type_` every time we extract
  // data from form no matter whether `ExtractCreditCard()` is
  // called or not.
  credit_card_import_type_ =
      payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard;
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_credit_card =
        ExtractCreditCard(submitted_form);
  }

#if !BUILDFLAG(IS_IOS)
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_iban =
        GetPaymentsFormDataImporter().ExtractIban(submitted_form);
  }
#endif  // !BUILDFLAG(IS_IOS)

  size_t num_complete_address_profiles = 0;
  if (profile_autofill_enabled &&
      !base::FeatureList::IsEnabled(features::kAutofillDisableAddressImport)) {
    num_complete_address_profiles =
        GetAddressFormDataImporter().ExtractAddressProfiles(
            submitted_form, &extracted_form_data.extracted_address_profiles);
  }

  if (profile_autofill_enabled && payment_methods_autofill_enabled) {
    const url::Origin origin = submitted_form.main_frame_origin();
    FormSignature form_signature = submitted_form.form_signature();
    // If multiple complete address profiles were extracted, this most likely
    // corresponds to billing and shipping sections within the same form.
    for (size_t i = 0; i < num_complete_address_profiles; ++i) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kAddressForm);
    }
    if (extracted_form_data.extracted_credit_card) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kCreditCardForm);
    }
  }

  return extracted_form_data;
}

bool FormDataImporter::ProcessExtractedCreditCard(
    const FormStructure& submitted_form,
    const std::optional<CreditCard>& extracted_credit_card,
    bool is_credit_card_upstream_enabled,
    ukm::SourceId ukm_source_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillPrioritizeSaveCardOverMandatoryReauth) &&
      payments_form_data_importer_
          .ProceedWithCardMandatoryReauthOptInIfApplicable()) {
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
      payments::PaymentsFormDataImporter::CreditCardImportType::kVirtualCard) {
    return base::FeatureList::IsEnabled(
               features::kAutofillPrioritizeSaveCardOverMandatoryReauth) &&
           payments_form_data_importer_
               .ProceedWithCardMandatoryReauthOptInIfApplicable();
  }

  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return false;
  }

  auto* virtual_card_enrollment_manager =
      client_->GetPaymentsAutofillClient()->GetVirtualCardEnrollmentManager();
  auto& context = GetPaymentsFormDataImporter().fetched_payments_data_context();
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
      payments_form_data_importer_
          .ProceedWithCardMandatoryReauthOptInIfApplicable()) {
    // Try to offer mandatory re-auth as the last step.
    return true;
  }

  return false;
}

std::optional<CreditCard> FormDataImporter::ExtractCreditCard(
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
  if (GetPaymentsFormDataImporter()
          .fetched_payments_data_context()
          .card_submitted_through_save_and_fill) {
    return std::nullopt;
  }

  // If the extracted card is a known virtual card, return the extracted card.
  if (GetPaymentsFormDataImporter().fetched_virtual_cards_.contains(
          candidate.LastFourDigits())) {
    credit_card_import_type_ =
        payments::PaymentsFormDataImporter::CreditCardImportType::kVirtualCard;
    return candidate;
  }

  // Can import one valid card per form. Start by treating it as kNewCard, but
  // overwrite this type if we discover it is already a local or server card.
  credit_card_import_type_ =
      payments::PaymentsFormDataImporter::CreditCardImportType::kNewCard;

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
      credit_card_import_type_ =
          payments::PaymentsFormDataImporter::CreditCardImportType::kLocalCard;
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
  return TryMatchingExistingServerCard(candidate);
}

std::optional<CreditCard> FormDataImporter::TryMatchingExistingServerCard(
    const CreditCard& candidate) {
  // Used for logging purposes later if we found a matching masked server card
  // with the same last four digits, but different expiration date as
  // `candidate`, and we treat it as a new card.
  bool same_last_four_but_different_expiration_date = false;

  for (const CreditCard* server_card :
       payments_data_manager().GetServerCreditCards()) {
    if (!server_card->HasSameNumberAs(candidate)) {
      continue;
    }

    // Cards with invalid expiration dates can be uploaded due to the existence
    // of the expiration date fix flow, however, since a server card with same
    // number is found, the imported card is treated as invalid card, abort
    // importing.
    if (!candidate.HasValidExpirationDate()) {
      return std::nullopt;
    }

    // Only return the masked server card if both the last four digits and
    // expiration date match.
    if (server_card->HasSameExpirationDateAs(candidate)) {
      AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
          AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED);

      // Return that we found a masked server card with matching last four
      // digits and copy over the user entered CVC so that future processing
      // logic check if CVC upload save should be offered.
      CreditCard server_card_with_cvc = *server_card;
      server_card_with_cvc.set_cvc(candidate.cvc());

      // If `credit_card_import_type_` was local card, then a local card was
      // extracted from the form. If a server card is now also extracted from
      // the form, the duplicate local and server card case is detected.
      if (credit_card_import_type_ == payments::PaymentsFormDataImporter::
                                          CreditCardImportType::kLocalCard) {
        credit_card_import_type_ = payments::PaymentsFormDataImporter::
            CreditCardImportType::kDuplicateLocalServerCard;
      } else {
        credit_card_import_type_ = payments::PaymentsFormDataImporter::
            CreditCardImportType::kServerCard;
      }
      return server_card_with_cvc;
    } else {
      // Keep track of the fact that we found a server card with matching
      // last four digits as `candidate`, but with a different expiration
      // date. If we do not end up finding a masked server card with
      // matching last four digits and the same expiration date as
      // `candidate`, we will later use
      // `same_last_four_but_different_expiration_date` for logging
      // purposes.
      same_last_four_but_different_expiration_date = true;
    }
  }

  // The only case that this is true is that we found a masked server card has
  // the same number as `candidate`, but with different expiration dates. Thus
  // we want to log this information once.
  if (same_last_four_but_different_expiration_date) {
    AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
        AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH);
  }

  return candidate;
}

payments::PaymentsFormDataImporter::ExtractCreditCardFromFormResult
FormDataImporter::ExtractCreditCardFromForm(const FormStructure& form) {
  // Populated by the lambdas below.
  payments::PaymentsFormDataImporter::ExtractCreditCardFromFormResult result;
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

void FormDataImporter::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  GetAddressFormDataImporter()
      .multi_step_import_merger()
      .OnBrowsingHistoryCleared(deletion_info);
  form_associator_.OnBrowsingHistoryCleared(deletion_info);
}

AddressFormDataImporter& FormDataImporter::GetAddressFormDataImporter() {
  return address_form_data_importer_;
}

payments::PaymentsFormDataImporter&
FormDataImporter::GetPaymentsFormDataImporter() {
  return payments_form_data_importer_;
}

PaymentsDataManager& FormDataImporter::payments_data_manager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill
