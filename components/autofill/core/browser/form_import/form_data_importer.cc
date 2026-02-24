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

  // TODO(crbug.com/481379161): Figure out a way to move this into
  // PaymentsFormDataImporter (potentially by passing
  // `preliminary_imported_address_profiles` into
  // PaymentsFormDataImporter::ProcessExtractedCreditCard()).
  GetPaymentsFormDataImporter()
      .GetCreditCardSaveManager()
      ->SetPreliminarilyImportedAutofillProfile(
          preliminary_imported_address_profiles);

  bool payments_prompt_potentially_shown = false;
  if (GetPaymentsFormDataImporter().ShouldProcessExtractedCreditCard()) {
    // Only check IsCreditCardUploadEnabled() if conditions that enable
    // processing of the extracted credit card are true, in order to prevent
    // the metrics it logs from being diluted by cases where extracted credit
    // cards should not be processed or there was no credit card to process.
    // TODO(crbug.com/481379161): Figure out a way to do this in
    // PaymentsFormDataImporter (potentially by moving it into
    // PaymentsFormDataImporter::ProcessExtractedCreditCard()).
    bool credit_card_upload_enabled = GetPaymentsFormDataImporter()
                                          .GetCreditCardSaveManager()
                                          ->IsCreditCardUploadEnabled();
    payments_prompt_potentially_shown =
        GetPaymentsFormDataImporter().ProcessExtractedCreditCard(
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
  // TODO(crbug.com/481379161): See if this can be moved into
  // PaymentsFormDataImporter::ExtractCreditCard().
  GetPaymentsFormDataImporter().credit_card_import_type_ =
      payments::PaymentsFormDataImporter::CreditCardImportType::kNoCard;
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_credit_card =
        GetPaymentsFormDataImporter().ExtractCreditCard(submitted_form);
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
      if (GetPaymentsFormDataImporter().credit_card_import_type_ ==
          payments::PaymentsFormDataImporter::CreditCardImportType::
              kLocalCard) {
        GetPaymentsFormDataImporter().credit_card_import_type_ =
            payments::PaymentsFormDataImporter::CreditCardImportType::
                kDuplicateLocalServerCard;
      } else {
        GetPaymentsFormDataImporter().credit_card_import_type_ = payments::
            PaymentsFormDataImporter::CreditCardImportType::kServerCard;
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
