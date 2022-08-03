// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_profile_save_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service.h"

namespace autofill {

using structured_address::VerificationStatus;

namespace {

using AddressImportRequirement =
    AutofillMetrics::AddressProfileImportRequirementMetric;

// Return true if the |field_type| and |value| are valid within the context
// of importing a form.
bool IsValidFieldTypeAndValue(const ServerFieldTypeSet types_seen,
                              ServerFieldType field_type,
                              const std::u16string& value,
                              LogBuffer* import_log_buffer) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for:
  // - EMAIL_ADDRESS because it is common to see second 'confirm email address'
  // field;
  // - PHONE_HOME_NUMBER because it is used to store both prefix and suffix of a
  // single number;
  // - phone number components because a form might request several phone
  // numbers.
  // TODO(crbug.com/1156315) Remove feature & PHONE_HOME_NUMBER checks when
  // launched.
  auto field_type_group = AutofillType(field_type).group();
  if (types_seen.count(field_type) && field_type != EMAIL_ADDRESS &&
      (base::FeatureList::IsEnabled(
           features::kAutofillEnableImportWhenMultiplePhoneNumbers)
           ? field_type_group != FieldTypeGroup::kPhoneBilling &&
                 field_type_group != FieldTypeGroup::kPhoneHome
           : field_type != PHONE_HOME_NUMBER)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Multiple fields of type "
        << AutofillType::ServerFieldTypeToString(field_type) << "." << CTag{};
    return false;
  }
  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Email address found in field of different type: "
        << AutofillType::ServerFieldTypeToString(field_type) << CTag{};
    return false;
  }

  return true;
}

// |imported_credit_card| refers to credit card that was most recently submitted
// and |fetched_card_instrument_id| refers to the instrument id of the most
// recently downstreamed (fetched from the server) credit card. These need to
// match to offer virtual card enrollment for the |imported_credit_card| .
bool ShouldOfferVirtualCardEnrollment(
    const CreditCard* imported_credit_card,
    absl::optional<int64_t> fetched_card_instrument_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableUpdateVirtualCardEnrollment)) {
    return false;
  }

  if (!imported_credit_card)
    return false;

  if (imported_credit_card->virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE) {
    return false;
  }

  if (!fetched_card_instrument_id.has_value() ||
      imported_credit_card->instrument_id() !=
          fetched_card_instrument_id.value()) {
    return false;
  }

  return true;
}

}  // namespace

FormDataImporter::FormDataImporter(AutofillClient* client,
                                   payments::PaymentsClient* payments_client,
                                   PersonalDataManager* personal_data_manager,
                                   const std::string& app_locale)
    : client_(client),
      credit_card_save_manager_(
          std::make_unique<CreditCardSaveManager>(client,
                                                  payments_client,
                                                  app_locale,
                                                  personal_data_manager)),
      address_profile_save_manager_(
          std::make_unique<AddressProfileSaveManager>(client,
                                                      personal_data_manager)),
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      local_card_migration_manager_(
          std::make_unique<LocalCardMigrationManager>(client,
                                                      payments_client,
                                                      app_locale,
                                                      personal_data_manager)),
      upi_vpa_save_manager_(
          std::make_unique<UpiVpaSaveManager>(client, personal_data_manager)),
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      personal_data_manager_(personal_data_manager),
      app_locale_(app_locale),
      virtual_card_enrollment_manager_(
          std::make_unique<VirtualCardEnrollmentManager>(personal_data_manager,
                                                         payments_client,
                                                         client)),
      multistep_importer_(app_locale,
                          client_->GetVariationConfigCountryCode()) {
  if (personal_data_manager_)
    personal_data_manager_->AddObserver(this);
}

FormDataImporter::~FormDataImporter() {
  if (personal_data_manager_)
    personal_data_manager_->RemoveObserver(this);
};

void FormDataImporter::ImportFormData(const FormStructure& submitted_form,
                                      bool profile_autofill_enabled,
                                      bool credit_card_autofill_enabled) {
  std::unique_ptr<CreditCard> imported_credit_card;
  absl::optional<std::string> detected_upi_id;

  std::vector<AddressProfileImportCandidate> address_profile_import_candidates;

  bool is_credit_card_upstream_enabled =
      credit_card_save_manager_->IsCreditCardUploadEnabled();

  ImportFormData(submitted_form, profile_autofill_enabled,
                 credit_card_autofill_enabled,
                 /*should_return_local_card=*/is_credit_card_upstream_enabled,
                 &imported_credit_card, address_profile_import_candidates,
                 &detected_upi_id);

  // Create a vector of address profile import candidates.
  // This is used to make preliminarily imported profiles available
  // to the credit card import logic.
  std::vector<AutofillProfile> preliminary_imported_address_profiles;
  for (const auto& candidate : address_profile_import_candidates) {
    if (candidate.all_requirements_fulfilled)
      preliminary_imported_address_profiles.push_back(candidate.profile);
  }
  credit_card_save_manager_->SetPreliminarilyImportedAutofillProfile(
      preliminary_imported_address_profiles);

  bool cc_prompt_potentially_shown = ProcessCreditCardImportCandidate(
      submitted_form, std::move(imported_credit_card), detected_upi_id,
      credit_card_autofill_enabled, is_credit_card_upstream_enabled);
  fetched_card_instrument_id_.reset();

  // If a prompt for credit cards is potentially shown, do not allow for a
  // second address profile import dialog.
  ProcessAddressProfileImportCandidates(address_profile_import_candidates,
                                        !cc_prompt_potentially_shown);
}

CreditCard FormDataImporter::ExtractCreditCardFromForm(
    const FormStructure& form) {
  bool has_duplicate_field_type;
  return ExtractCreditCardFromForm(form, &has_duplicate_field_type);
}

// static
bool FormDataImporter::IsValidLearnableProfile(
    const AutofillProfile& profile,
    const std::string& predicted_country_code,
    const std::string& app_locale,
    LogBuffer* import_log_buffer) {
  // Check that the email address is valid if it is supplied.
  bool is_email_invalid = false;
  std::u16string email = profile.GetRawInfo(EMAIL_ADDRESS);
  if (!email.empty() && !IsValidEmailAddress(email)) {
    LOG_AF(import_log_buffer) << LogMessage::kImportAddressProfileFromFormFailed
                              << "Invalid email address." << CTag{};
    is_email_invalid = true;
  }

  // Reject profiles with an invalid |HOME_ADDRESS_STATE| entry.
  bool is_state_invalid = false;
  if (profile.IsPresentButInvalid(ADDRESS_HOME_STATE)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Invalid state as of AutofillProfile::IsPresentButInvalid()."
        << CTag{};
    is_state_invalid = true;
  }

  // Reject profiles with an invalid |HOME_ADDRESS_ZIP| entry.
  bool is_zip_invalid = false;
  if (profile.IsPresentButInvalid(ADDRESS_HOME_ZIP)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Invalid ZIP as of AutofillProfile::IsPresentButInvalid()."
        << CTag{};
    is_zip_invalid = true;
  }

  // Collect metrics.
  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_email_invalid
          ? AddressImportRequirement::EMAIL_VALID_REQUIREMENT_VIOLATED
          : AddressImportRequirement::EMAIL_VALID_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_state_invalid
          ? AddressImportRequirement::STATE_VALID_REQUIREMENT_VIOLATED
          : AddressImportRequirement::STATE_VALID_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_zip_invalid
          ? AddressImportRequirement::ZIP_VALID_REQUIREMENT_VIOLATED
          : AddressImportRequirement::ZIP_VALID_REQUIREMENT_FULFILLED);

  // Return true if none of the requirements is violated.
  return !(is_email_invalid || is_state_invalid || is_zip_invalid);
}

bool FormDataImporter::ComplementCountry(
    AutofillProfile& profile,
    const std::string& predicted_country_code) {
  // TODO(crbug.com/1297032): Cleanup `kAutofillComplementCountryCodeOnImport`
  // check when launched.
  bool should_complement_country =
      !profile.HasRawInfo(ADDRESS_HOME_COUNTRY) &&
      base::FeatureList::IsEnabled(
          features::kAutofillAddressProfileSavePrompt) &&
      base::FeatureList::IsEnabled(
          features::kAutofillComplementCountryCodeOnImport);
  return should_complement_country &&
         profile.SetInfoWithVerificationStatus(
             AutofillType(ADDRESS_HOME_COUNTRY),
             base::ASCIIToUTF16(predicted_country_code), app_locale_,
             VerificationStatus::kObserved);
}

bool FormDataImporter::SetPhoneNumber(
    AutofillProfile& profile,
    PhoneNumber::PhoneCombineHelper& combined_phone,
    const std::string& predicted_country_code) {
  if (combined_phone.IsEmpty()) {
    return true;
  }
  const std::string predicted_country_code_without_variation =
      GetPredictedCountryCode(profile, "", app_locale_, nullptr);
  auto SetWithRegion = [&](const std::string& region) {
    std::u16string constructed_number;
    // `ParseNumber()` implicity accepts both a country code and a locale. This
    // will be refactored with crbug.com/1296077. The parameter for
    // `SetInfoWithVerificationStatus()` has to be consistent with
    // `ParseNumber()`.
    return combined_phone.ParseNumber(profile, region, &constructed_number) &&
           profile.SetInfoWithVerificationStatus(
               PHONE_HOME_WHOLE_NUMBER, constructed_number,
               /*app_locale=*/region, VerificationStatus::kObserved);
  };
  // If `AutofillConsiderVariationCountryCodeForPhoneNumbers` is enabled,
  // a consistent country code prediction for addresses and phone numbers is
  // used. Otherwise the variation service state is not considered for phone
  // numbers. This makes a difference, if the country code cannot be found
  // in the `profile`.
  // TODO(crbug.com/1295721): Cleanup when launched.
  bool success_with_locale = SetWithRegion(app_locale_);
  if (predicted_country_code == predicted_country_code_without_variation ||
      !base::FeatureList::IsEnabled(
          features::kAutofillConsiderVariationCountryCodeForPhoneNumbers))
    return success_with_locale;
  // AutofillConsiderVariationCountryCodeForPhoneNumbers is enabled and makes
  // a difference for the region used. Parse the number with the new region and
  // check if this actually changes the parsing outcome to measure the impact.
  bool success_with_variation_code = SetWithRegion(predicted_country_code);
  AutofillMetrics::LogPhoneNumberImportParsingResult(
      success_with_variation_code, success_with_locale);
  // Keep the current state, even if the parsing worked with the locale but not
  // the variation country code. Because once
  // `AutofillConsiderVariationCountryCodeForPhoneNumbers` is launched, only
  // region = `predicted_country_code` will be used for parsing.
  return success_with_variation_code;
}

void FormDataImporter::RemoveInaccessibleProfileValues(
    AutofillProfile& profile,
    const std::string& predicted_country_code) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillRemoveInaccessibleProfileValues)) {
    const ServerFieldTypeSet inaccessible_fields =
        profile.FindInaccessibleProfileValues(predicted_country_code);
    profile.ClearFields(inaccessible_fields);
    AutofillMetrics::LogRemovedSettingInaccessibleFields(
        !inaccessible_fields.empty());
    for (const ServerFieldType inaccessible_field : inaccessible_fields) {
      AutofillMetrics::LogRemovedSettingInaccessibleField(inaccessible_field);
    }
  }
}

void FormDataImporter::CacheFetchedVirtualCard(
    const std::u16string& last_four) {
  fetched_virtual_cards_.insert(last_four);
}

void FormDataImporter::SetFetchedCardInstrumentId(int64_t instrument_id) {
  fetched_card_instrument_id_ = instrument_id;
}

bool FormDataImporter::ImportFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool credit_card_autofill_enabled,
    bool should_return_local_card,
    std::unique_ptr<CreditCard>* imported_credit_card,
    std::vector<AddressProfileImportCandidate>&
        address_profile_import_candidates,
    absl::optional<std::string>* imported_upi_id) {
  // We try the same |form| for both credit card and address import/update.
  // - ImportCreditCard may update an existing card, or fill
  //   |imported_credit_card| with an extracted card. See .h for details of
  //   |should_return_local_card|.
  // Reset |imported_credit_card_record_type_| every time we import data from
  // form no matter whether ImportCreditCard() is called or not.
  imported_credit_card_record_type_ = ImportedCreditCardRecordType::NO_CARD;
  bool cc_import = false;
  if (credit_card_autofill_enabled) {
    cc_import = ImportCreditCard(submitted_form, should_return_local_card,
                                 imported_credit_card);
    *imported_upi_id = ImportUpiId(submitted_form);
  }
  // - ImportAddressProfiles may eventually save or update one or more address
  //   profiles.
  bool address_import = false;

  // Only import addresses if enabled.
  if (profile_autofill_enabled &&
      !base::FeatureList::IsEnabled(features::kAutofillDisableAddressImport)) {
    address_import = ImportAddressProfiles(submitted_form,
                                           address_profile_import_candidates);
  }

  if (cc_import || address_import || imported_upi_id->has_value())
    return true;

  personal_data_manager_->MarkObserversInsufficientFormDataForImport();
  return false;
}

bool FormDataImporter::ImportAddressProfiles(
    const FormStructure& form,
    std::vector<AddressProfileImportCandidate>& import_candidates) {
  // Create a buffer to collect logging output for the autofill-internals.
  LogManager* log_manager = client_->GetLogManager();
  LogBuffer import_log_buffer(IsLoggingActive(log_manager));
  LOG_AF(import_log_buffer) << LoggingScope::kAddressProfileFormImport;
  // Print the full form into the logging scope.
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromForm << form << CTag{};

  // We save a maximum of 2 profiles per submitted form (e.g. for shipping and
  // billing).
  static const size_t kMaxNumAddressProfilesSaved = 2;
  size_t num_complete_profiles = 0;

  if (!form.field_count()) {
    LOG_AF(import_log_buffer) << LogMessage::kImportAddressProfileFromFormFailed
                              << "Form is empty." << CTag{};
  } else {
    // Relevant sections for address fields.
    std::set<Section> sections;
    for (const auto& field : form) {
      if (field->Type().group() != FieldTypeGroup::kCreditCard)
        sections.insert(field->section);
    }

    for (const Section& section : sections) {
      if (num_complete_profiles == kMaxNumAddressProfilesSaved)
        break;
      // Log the output from a section in a separate div for readability.
      LOG_AF(import_log_buffer)
          << Tag{"div"} << Attrib{"class", "profile_import_from_form_section"};
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormSection << section
          << CTag{};
      // Try to import an address profile from the form fields of this section.
      // Only allow for a prompt if no other complete profile was found so far.
      if (ImportAddressProfileForSection(form, section, import_candidates,
                                         &import_log_buffer))
        num_complete_profiles++;
      // And close the div of the section import log.
      LOG_AF(import_log_buffer) << CTag{"div"};
    }
    // Run the import on the union of the section if the import was not
    // successful and if there is more than one section.
    if (num_complete_profiles > 0) {
      AutofillMetrics::LogAddressFormImportStatusMetric(
          AutofillMetrics::AddressProfileImportStatusMetric::REGULAR_IMPORT);
    } else if (sections.size() > 1) {
      // Try to import by combining all sections.
      if (ImportAddressProfileForSection(form, absl::nullopt, import_candidates,
                                         &import_log_buffer)) {
        num_complete_profiles++;
        AutofillMetrics::LogAddressFormImportStatusMetric(
            AutofillMetrics::AddressProfileImportStatusMetric::
                SECTION_UNION_IMPORT);
      }
    }
    if (num_complete_profiles == 0) {
      AutofillMetrics::LogAddressFormImportStatusMetric(
          AutofillMetrics::AddressProfileImportStatusMetric::NO_IMPORT);
    }
  }
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromFormNumberOfImports
      << num_complete_profiles << CTag{};

  // Write log buffer to autofill-internals.
  LOG_AF(log_manager) << std::move(import_log_buffer);

  return num_complete_profiles > 0;
}

bool FormDataImporter::ImportAddressProfileForSection(
    const FormStructure& form,
    const absl::optional<Section>& section,
    std::vector<AddressProfileImportCandidate>& import_candidates,
    LogBuffer* import_log_buffer) {
  // The candidate for profile import. There are many ways for the candidate to
  // be rejected (see everywhere this function returns false).
  AutofillProfile candidate_profile;

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper combined_phone;

  // Used to detect and discard address forms with multiple fields of the same
  // type.
  ServerFieldTypeSet types_seen;

  // Tracks if the form section contains multiple distinct email addresses.
  bool has_multiple_distinct_email_addresses = false;

  // Tracks if the form section contains an invalid types.
  bool has_invalid_field_types = false;

  // Tracks if the form section contains an invalid phone number.
  bool has_invalid_phone_number = false;

  // Tracks if the form section contains an invalid country.
  bool has_invalid_country = false;

  // Tracks if subsequent phone number fields should be ignored,
  // since they do not belong to the first phone number in the form.
  bool ignore_phone_number_fields = false;

  // Metadata about the way we construct candidate_profile.
  ProfileImportMetadata import_metadata;

  // Tracks if any of the fields belongs to FormType::kAddressForm.
  bool has_address_related_fields = false;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const auto& field : form) {
    // Reject fields that are not within the specified |section|.
    // If no section is passed, use all fields.
    if (section && field->section != *section)
      continue;

    std::u16string value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, or the field is non-focusable (hidden), then
    // skip it.
    // TODO(crbug.com/1101280): Remove |skip_unfocussable_field|
    bool skip_unfocussable_field =
        !field->is_focusable &&
        !base::FeatureList::IsEnabled(
            features::kAutofillProfileImportFromUnfocusableFields);
    if (!field->IsFieldFillable() || skip_unfocussable_field || value.empty())
      continue;

    AutofillType field_type = field->Type();

    // Credit card fields are handled by ImportCreditCard().
    if (field_type.group() == FieldTypeGroup::kCreditCard)
      continue;

    has_address_related_fields |=
        FieldTypeGroupToFormType(field_type.group()) == FormType::kAddressForm;

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    ServerFieldType server_field_type = field_type.GetStorableType();
    if (server_field_type == EMAIL_ADDRESS &&
        types_seen.count(server_field_type) &&
        candidate_profile.GetRawInfo(EMAIL_ADDRESS) != value) {
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormFailed
          << "Multiple different email addresses present." << CTag{};
      has_multiple_distinct_email_addresses = true;
    }

    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(types_seen, server_field_type, value,
                                  import_log_buffer))
      has_invalid_field_types = true;

    // Found phone number component field.
    // TODO(crbug.com/1156315) Remove feature check when launched.
    if ((field_type.group() == FieldTypeGroup::kPhoneBilling ||
         field_type.group() == FieldTypeGroup::kPhoneHome) &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
      if (ignore_phone_number_fields)
        continue;
      // PHONE_HOME_NUMBER is used for both prefix and suffix, so it might occur
      // multiple times for a single number. Duplication of any other phone
      // component means it belongs to a new number. Since Autofill currently
      // supports storing only one phone number per profile, ignore this and all
      // subsequent phone number fields.
      if (server_field_type != PHONE_HOME_NUMBER &&
          types_seen.count(server_field_type)) {
        ignore_phone_number_fields = true;
        continue;
      }
    }

    types_seen.insert(server_field_type);

    // We need to store phone data in the variables, before building the whole
    // number at the end. If |value| is not from a phone field, home.SetInfo()
    // returns false and data is stored directly in |candidate_profile|.
    if (!combined_phone.SetInfo(field_type, value)) {
      candidate_profile.SetInfoWithVerificationStatus(
          field_type, value, app_locale_, VerificationStatus::kObserved);
    }

    // Reject profiles with invalid country information.
    if (server_field_type == ADDRESS_HOME_COUNTRY &&
        !candidate_profile.HasRawInfo(ADDRESS_HOME_COUNTRY)) {
      // The country code was not successfully determined from the value in
      // the country field. This can be caused by a localization that does not
      // match the |app_locale|. Try setting the value again using the
      // language of the page. Note, there should be a locale associated with
      // every language code.
      std::string page_language;
      const translate::LanguageState* language_state =
          client_->GetLanguageState();
      if (language_state)
        page_language = language_state->source_language();
      // Retry to set the country of there is known page language.
      if (!page_language.empty()) {
        candidate_profile.SetInfoWithVerificationStatus(
            field_type, value, page_language, VerificationStatus::kObserved);
      }
      // Check if the country code was still not determined correctly.
      if (!candidate_profile.HasRawInfo(ADDRESS_HOME_COUNTRY)) {
        LOG_AF(import_log_buffer)
            << LogMessage::kImportAddressProfileFromFormFailed
            << "Missing country." << CTag{};
        has_invalid_country = true;
      }
    }
  }

  const std::string variation_country_code =
      client_->GetVariationConfigCountryCode();
  std::string predicted_country_code =
      GetPredictedCountryCode(candidate_profile, variation_country_code,
                              app_locale_, import_log_buffer);

  if (!SetPhoneNumber(candidate_profile, combined_phone,
                      predicted_country_code)) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillRemoveInvalidPhoneNumberOnImport)) {
      candidate_profile.ClearFields({PHONE_HOME_WHOLE_NUMBER});
      import_metadata.did_remove_invalid_phone_number = true;
    } else {
      has_invalid_phone_number = true;
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormFailed
          << "Invalid phone number." << CTag{};
    }
  }

  // This is done prior to checking the validity of the profile, because multi-
  // step import profile merging requires the profile to be finalized. Ideally
  // we would return false here if it fails, but that breaks the metrics.
  bool finalized_import = candidate_profile.FinalizeAfterImport();

  // Reject the profile if the validation requirements are not met.
  // |IsValidLearnableProfile()| goes first to collect metrics.
  bool has_invalid_information =
      !IsValidLearnableProfile(candidate_profile, predicted_country_code,
                               app_locale_, import_log_buffer) ||
      has_multiple_distinct_email_addresses || has_invalid_field_types ||
      has_invalid_country || has_invalid_phone_number;

  // Profiles with valid information qualify for multi-step imports.
  // This requires the profile to be finalized to apply the merging logic.
  if (finalized_import && has_address_related_fields &&
      !has_invalid_information) {
    multistep_importer_.ProcessMultiStepImport(
        candidate_profile, import_metadata,
        url::Origin::Create(form.source_url()));
    // The predicted country code has possibly changed, if |candidate_profile|
    // was merged with a profile containing country information.
    predicted_country_code =
        GetPredictedCountryCode(candidate_profile, variation_country_code,
                                app_locale_, /*import_log_buffer=*/nullptr);
  }

  // Only complement the country if no invalid country was entered in the form.
  // For multi-step imports, |did_complement_country| might be set twice, but as
  // the metric is only logged if it wasn't present before, this is fine.
  import_metadata.did_complement_country =
      !has_invalid_country &&
      ComplementCountry(candidate_profile, predicted_country_code);

  RemoveInaccessibleProfileValues(candidate_profile, predicted_country_code);

  // Do not import a profile if any of the requirements is violated.
  // |IsMinimumAddress()| goes first to collect metrics.
  bool all_fulfilled =
      IsMinimumAddress(candidate_profile, predicted_country_code, app_locale_,
                       import_log_buffer, /*collect_metrics=*/true) &&
      !has_invalid_information;

  // Collect metrics regarding the requirements for an address profile import.
  AutofillMetrics::LogAddressFormImportRequirementMetric(
      has_multiple_distinct_email_addresses
          ? AddressImportRequirement::EMAIL_ADDRESS_UNIQUE_REQUIREMENT_VIOLATED
          : AddressImportRequirement::
                EMAIL_ADDRESS_UNIQUE_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      has_invalid_field_types
          ? AddressImportRequirement::
                NO_INVALID_FIELD_TYPES_REQUIREMENT_VIOLATED
          : AddressImportRequirement::
                NO_INVALID_FIELD_TYPES_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      has_invalid_phone_number
          ? AddressImportRequirement::PHONE_VALID_REQUIREMENT_VIOLATED
          : AddressImportRequirement::PHONE_VALID_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      has_invalid_country
          ? AddressImportRequirement::COUNTRY_VALID_REQUIREMENT_VIOLATED
          : AddressImportRequirement::COUNTRY_VALID_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      all_fulfilled ? AddressImportRequirement::OVERALL_REQUIREMENT_FULFILLED
                    : AddressImportRequirement::OVERALL_REQUIREMENT_VIOLATED);

  bool candidate_has_structured_data =
      base::FeatureList::IsEnabled(
          features::kAutofillSilentProfileUpdateForInsufficientImport) &&
      candidate_profile.HasStructuredData();

  // If the profile does not fulfill import requirements but contains the
  // structured address or name information, it is eligible for silently
  // updating the existing profiles.
  if (!finalized_import || (!all_fulfilled && !candidate_has_structured_data))
    return false;

  // At this stage, the saving of the profile can only be omitted by the
  // incognito mode but the import is not triggered if the browser is in the
  // incognito mode.
  DCHECK(!personal_data_manager_->IsOffTheRecord());

  import_candidates.push_back(
      AddressProfileImportCandidate{.profile = candidate_profile,
                                    .url = form.source_url(),
                                    .all_requirements_fulfilled = all_fulfilled,
                                    .import_metadata = import_metadata});

  // Return true if a compelete importable profile was found.
  return all_fulfilled;
}

bool FormDataImporter::ProcessAddressProfileImportCandidates(
    const std::vector<AddressProfileImportCandidate>& import_candidates,
    bool allow_prompt) {
  // At this point, no credit card prompt was shown. Initiate the import of
  // addresses is possible.
  int imported_profiles = 0;
  if (allow_prompt || !base::FeatureList::IsEnabled(
                          features::kAutofillAddressProfileSavePrompt)) {
    for (const auto& candidate : import_candidates) {
      // First try to import a single complete profile.
      if (!candidate.all_requirements_fulfilled)
        continue;
      address_profile_save_manager_->ImportProfileFromForm(
          candidate.profile, app_locale_, candidate.url,
          /*allow_only_silent_updates=*/false, candidate.import_metadata);
      // Limit the number of importable profiles to 2.
      if (++imported_profiles >= 2)
        return true;
    }
  }
  // If a profile was already imported, do not try to use partial profiles for
  // silent updates.
  if (imported_profiles > 0)
    return true;
  // Otherwise try again but restrict the import to silent updates.
  for (const auto& candidate : import_candidates) {
    // First try to import a single complete profile.
    address_profile_save_manager_->ImportProfileFromForm(
        candidate.profile, app_locale_, candidate.url,
        /*allow_only_silent_updates=*/true, candidate.import_metadata);
  }
  return false;
}

bool FormDataImporter::ProcessCreditCardImportCandidate(
    const FormStructure& submitted_form,
    std::unique_ptr<CreditCard> imported_credit_card,
    absl::optional<std::string> detected_upi_id,
    bool credit_card_autofill_enabled,
    bool is_credit_card_upstream_enabled) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (detected_upi_id && credit_card_autofill_enabled &&
      base::FeatureList::IsEnabled(features::kAutofillSaveAndFillVPA)) {
    upi_vpa_save_manager_->OfferLocalSave(*detected_upi_id);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // If no card was successfully imported from the form, return.
  if (imported_credit_card_record_type_ ==
      ImportedCreditCardRecordType::NO_CARD) {
    return false;
  }
  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return false;
  }

  // Do not offer credit card save at all if Autofill Assistant is running.
  if (client_->IsAutofillAssistantShowing())
    return false;

  if (ShouldOfferVirtualCardEnrollment(imported_credit_card.get(),
                                       fetched_card_instrument_id_)) {
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *imported_credit_card, VirtualCardEnrollmentSource::kDownstream);
    return true;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // A credit card was successfully imported, but it's possible it is already a
  // local or server card. First, check to see if we should offer local card
  // migration in this case, as local cards could go either way.
  if (local_card_migration_manager_ &&
      local_card_migration_manager_->ShouldOfferLocalCardMigration(
          imported_credit_card.get(), imported_credit_card_record_type_)) {
    local_card_migration_manager_->AttemptToOfferLocalCardMigration(
        /*is_from_settings_page=*/false);
    return true;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Local card migration will not be offered. We check to see if it is valid to
  // offer upload save or local card save, which will happen below if we do not
  // early return false in this if-statement.
  if (!ShouldOfferUploadCardOrLocalCardSave(imported_credit_card.get(),
                                            is_credit_card_upstream_enabled)) {
    return false;
  }

  // We have a card to save; decide what type of save flow to display.
  if (is_credit_card_upstream_enabled) {
    // Attempt to offer upload save. Because we pass
    // |credit_card_upstream_enabled| to ImportFormData, this block can be
    // reached on observing either a new card or one already stored locally
    // which doesn't match an existing server card. If Google Payments declines
    // allowing upload, |credit_card_save_manager_| is tasked with deciding if
    // we should fall back to local save or not.
    DCHECK(imported_credit_card_record_type_ ==
               ImportedCreditCardRecordType::LOCAL_CARD ||
           imported_credit_card_record_type_ ==
               ImportedCreditCardRecordType::NEW_CARD);
    credit_card_save_manager_->AttemptToOfferCardUploadSave(
        submitted_form, from_dynamic_change_form_, has_non_focusable_field_,
        *imported_credit_card,
        /*uploading_local_card=*/imported_credit_card_record_type_ ==
            ImportedCreditCardRecordType::LOCAL_CARD);
    return true;
  };
  // If upload save is not allowed, new cards should be saved locally.
  DCHECK(imported_credit_card_record_type_ ==
         ImportedCreditCardRecordType::NEW_CARD);
  if (credit_card_save_manager_->AttemptToOfferCardLocalSave(
          from_dynamic_change_form_, has_non_focusable_field_,
          *imported_credit_card)) {
    return true;
  }

  return false;
}

bool FormDataImporter::ImportCreditCard(
    const FormStructure& form,
    bool should_return_local_card,
    std::unique_ptr<CreditCard>* imported_credit_card) {
  DCHECK(!*imported_credit_card);

  // The candidate for credit card import. There are many ways for the candidate
  // to be rejected (see everywhere this function returns false, below).
  bool has_duplicate_field_type;
  CreditCard candidate_credit_card =
      ExtractCreditCardFromForm(form, &has_duplicate_field_type);

  // If we've seen the same credit card field type twice in the same form,
  // abort credit card import/update.
  if (has_duplicate_field_type)
    return false;

  if (candidate_credit_card.IsValid()) {
    AutofillMetrics::LogSubmittedCardStateMetric(
        AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE);
  } else {
    if (candidate_credit_card.HasValidCardNumber()) {
      AutofillMetrics::LogSubmittedCardStateMetric(
          AutofillMetrics::HAS_CARD_NUMBER_ONLY);
    }
    if (candidate_credit_card.HasValidExpirationDate()) {
      AutofillMetrics::LogSubmittedCardStateMetric(
          AutofillMetrics::HAS_EXPIRATION_DATE_ONLY);
    }
  }

  // Cards with invalid expiration dates can be uploaded due to the existence of
  // the expiration date fix flow. However, cards with invalid card numbers must
  // still be ignored.
  if (!candidate_credit_card.HasValidCardNumber()) {
    return false;
  }

  // If the imported card is a known virtual card, abort importing.
  if (fetched_virtual_cards_.contains(candidate_credit_card.LastFourDigits()))
    return false;

  // Can import one valid card per form. Start by treating it as NEW_CARD, but
  // overwrite this type if we discover it is already a local or server card.
  imported_credit_card_record_type_ = ImportedCreditCardRecordType::NEW_CARD;

  // Denotes whether the extracted card matches a local card. Used to help
  // determine the return value of this function for use by tests. This will be
  // used to ensure if we found a matched local card and
  // |should_return_local_card| is false, that we return true so that this
  // function matches the legacy implementation.
  // TODO(crbug.com/1291243): Deprecate returning bool values.
  bool matched_local_card = false;

  // Attempt to merge with an existing credit card. Don't present a prompt if we
  // have already saved this card number, unless |should_return_local_card| is
  // true which indicates that upload is enabled. In this case, it's useful to
  // present the upload prompt to the user to promote the card from a local card
  // to a synced server card, provided we don't have a masked server card with
  // the same |TypeAndLastFourDigits|.
  for (const CreditCard* card : personal_data_manager_->GetLocalCreditCards()) {
    // Make a local copy so that the data in |local_credit_cards_| isn't
    // modified directly by the UpdateFromImportedCard() call.
    CreditCard card_copy(*card);
    if (card_copy.UpdateFromImportedCard(candidate_credit_card, app_locale_)) {
      matched_local_card = true;
      personal_data_manager_->UpdateCreditCard(card_copy);
      // Mark that the credit card imported from the submitted form is
      // already a local card.
      imported_credit_card_record_type_ =
          ImportedCreditCardRecordType::LOCAL_CARD;

      // If the card is a local card and it has a nickname stored in the local
      // database, copy the nickname to the |candidate_credit_card| so that the
      // nickname also shows in the Upstream bubble.
      candidate_credit_card.SetNickname(card_copy.nickname());
    }
  }

  // If we are able to find a matching server card for the imported card, we set
  // |imported_credit_card_record_type_| to SERVER_CARD, and set
  // |imported_credit_card| to point to the corresponding CreditCard. Note: if a
  // local card was found in the previous for-loop, this will override
  // |imported_credit_card| to the server card data (it would previously be set
  // to the local card data) as we want the server to be the source of truth.
  for (const CreditCard* card :
       personal_data_manager_->GetServerCreditCards()) {
    if ((card->record_type() == CreditCard::MASKED_SERVER_CARD &&
         card->LastFourDigits() == candidate_credit_card.LastFourDigits()) ||
        (card->record_type() == CreditCard::FULL_SERVER_CARD &&
         candidate_credit_card.HasSameNumberAs(*card))) {
      // Don't import the card if the expiration date is missing.
      if (candidate_credit_card.expiration_month() == 0 ||
          candidate_credit_card.expiration_year() == 0) {
        return false;
      }
      // Mark that the imported credit card is a server card.
      imported_credit_card_record_type_ =
          ImportedCreditCardRecordType::SERVER_CARD;
      // Record metric on whether expiration dates matched.
      if (candidate_credit_card.expiration_month() ==
              card->expiration_month() &&
          candidate_credit_card.expiration_year() == card->expiration_year()) {
        AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
            card->record_type() == CreditCard::FULL_SERVER_CARD
                ? AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED
                : AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED);
      } else {
        AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
            card->record_type() == CreditCard::FULL_SERVER_CARD
                ? AutofillMetrics::
                      FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH
                : AutofillMetrics::
                      MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH);
      }
      // We found a server card that matches the data in the form. Set
      // |imported_credit_card| to point to the corresponding CreditCard so that
      // a future flow that would need this data can use it (such as virtual
      // card enrollment flow).
      *imported_credit_card = std::make_unique<CreditCard>(*card);

      return matched_local_card && !should_return_local_card;
    }
  }
  *imported_credit_card = std::make_unique<CreditCard>(candidate_credit_card);
  return true;
}

CreditCard FormDataImporter::ExtractCreditCardFromForm(
    const FormStructure& form,
    bool* has_duplicate_field_type) {
  *has_duplicate_field_type = false;
  has_non_focusable_field_ = false;
  from_dynamic_change_form_ = false;

  CreditCard candidate_credit_card;

  ServerFieldTypeSet types_seen;
  for (const auto& field : form) {
    std::u16string value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty())
      continue;

    AutofillType field_type = field->Type();
    // Field was not identified as a credit card field.
    if (field_type.group() != FieldTypeGroup::kCreditCard)
      continue;

    if (form.value_from_dynamic_change_form())
      from_dynamic_change_form_ = true;
    if (!field->is_focusable)
      has_non_focusable_field_ = true;

    // If we've seen the same credit card field type twice in the same form,
    // set |has_duplicate_field_type| to true.
    ServerFieldType server_field_type = field_type.GetStorableType();
    if (types_seen.count(server_field_type)) {
      *has_duplicate_field_type = true;
    } else {
      types_seen.insert(server_field_type);
    }
    // If |field| is an HTML5 month input, handle it as a special case.
    if (base::EqualsCaseInsensitiveASCII(field->form_control_type, "month")) {
      DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, server_field_type);
      candidate_credit_card.SetInfoForMonthInputType(value);
      continue;
    }

    // CreditCard handles storing the |value| according to |field_type|.
    bool saved = candidate_credit_card.SetInfo(field_type, value, app_locale_);

    // Saving with the option text (here |value|) may fail for the expiration
    // month. Attempt to save with the option value. First find the index of the
    // option text in the select options and try the corresponding value.
    if (!saved && server_field_type == CREDIT_CARD_EXP_MONTH) {
      for (const SelectOption& option : field->options) {
        if (value == option.content) {
          candidate_credit_card.SetInfo(field_type, option.value, app_locale_);
          break;
        }
      }
    }
  }

  return candidate_credit_card;
}

absl::optional<std::string> FormDataImporter::ImportUpiId(
    const FormStructure& form) {
  for (const auto& field : form) {
    if (IsUPIVirtualPaymentAddress(field->value))
      return base::UTF16ToUTF8(field->value);
  }
  return absl::nullopt;
}

bool FormDataImporter::ShouldOfferUploadCardOrLocalCardSave(
    const CreditCard* imported_credit_card,
    bool is_credit_card_upload_enabled) {
  // If we have an invalid card in the form, a duplicate field type, or we have
  // entered a virtual card, |imported_credit_card| will be set
  // to nullptr and thus we do not want to offer upload save or local card save.
  if (!imported_credit_card)
    return false;

  // We do not want to offer upload save or local card save for server cards.
  if (imported_credit_card_record_type_ ==
      ImportedCreditCardRecordType::SERVER_CARD) {
    return false;
  }

  // If we have a local card but credit card upload is not enabled, we do not
  // want to offer upload save as it is disabled and we do not want to offer
  // local card save as it is already saved as a local card.
  if (!is_credit_card_upload_enabled &&
      imported_credit_card_record_type_ ==
          ImportedCreditCardRecordType::LOCAL_CARD) {
    return false;
  }

  // We know |imported_credit_card| is either a new card, or a local card with
  // upload enabled.
  return true;
}

void FormDataImporter::OnBrowsingHistoryCleared(
    const history::DeletionInfo& deletion_info) {
  // Delete all multi-step import candidates when:
  // - The entire browsing history is cleared, or
  // - At least one URL from the same origin as `multistep_importer_`
  //   is deleted.
  if (deletion_info.IsAllHistory() ||
      (multistep_importer_.Origin() &&
       base::Contains(deletion_info.deleted_rows(),
                      *multistep_importer_.Origin(),
                      [](const history::URLRow& url_row) {
                        return url::Origin::Create(url_row.url());
                      }))) {
    ClearMultiStepImportCandidates();
  }
}

}  // namespace autofill
