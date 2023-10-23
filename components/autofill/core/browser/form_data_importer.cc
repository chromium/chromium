// Copyright 2017 The Chromium Authors
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

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_profile_save_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/plus_addresses/plus_address_service.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

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
  // - phone number components because a form might request several phone
  // numbers.
  // TODO(crbug.com/1156315) Clean up when launched.
  FieldTypeGroup field_type_group = GroupTypeOfServerFieldType(field_type);
  if (types_seen.count(field_type) && field_type != EMAIL_ADDRESS &&
      (!base::FeatureList::IsEnabled(
           features::kAutofillEnableImportWhenMultiplePhoneNumbers) ||
       field_type_group != FieldTypeGroup::kPhone)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Multiple fields of type " << FieldTypeToStringView(field_type)
        << "." << CTag{};
    return false;
  }
  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value)) {
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormFailed
        << "Email address found in field of different type: "
        << FieldTypeToStringView(field_type) << CTag{};
    return false;
  }

  return true;
}

// `extracted_credit_card` refers to the credit card that was most recently
// submitted and |fetched_card_instrument_id| refers to the instrument id of the
// most recently downstreamed (fetched from the server) credit card.
// These need to match to offer virtual card enrollment for the
// `extracted_credit_card`.
bool ShouldOfferVirtualCardEnrollment(
    const absl::optional<CreditCard>& extracted_credit_card,
    absl::optional<int64_t> fetched_card_instrument_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableUpdateVirtualCardEnrollment)) {
    return false;
  }

  if (!extracted_credit_card) {
    return false;
  }

  if (extracted_credit_card->virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible) {
    return false;
  }

  if (!fetched_card_instrument_id.has_value() ||
      extracted_credit_card->instrument_id() !=
          fetched_card_instrument_id.value()) {
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
      iban_save_manager_(
          std::make_unique<IbanSaveManager>(client, personal_data_manager)),
      local_card_migration_manager_(
          std::make_unique<LocalCardMigrationManager>(client,
                                                      payments_client,
                                                      app_locale,
                                                      personal_data_manager)),
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
}

void FormDataImporter::set_credit_card_save_manager_for_testing(
    std::unique_ptr<CreditCardSaveManager> credit_card_save_manager) {
  credit_card_save_manager_ = std::move(credit_card_save_manager);
}

FormDataImporter::AddressProfileImportCandidate::
    AddressProfileImportCandidate() = default;
FormDataImporter::AddressProfileImportCandidate::AddressProfileImportCandidate(
    const FormDataImporter::AddressProfileImportCandidate& other) = default;
FormDataImporter::AddressProfileImportCandidate::
    ~AddressProfileImportCandidate() = default;

void FormDataImporter::ImportAndProcessFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool payment_methods_autofill_enabled) {
  ExtractedFormData extracted_data =
      ExtractFormData(submitted_form, profile_autofill_enabled,
                      payment_methods_autofill_enabled);

  // Create a vector of address profile import candidates.
  // This is used to make preliminarily imported profiles available
  // to the credit card import logic.
  std::vector<AutofillProfile> preliminary_imported_address_profiles;
  for (const auto& candidate :
       extracted_data.address_profile_import_candidates) {
    if (candidate.all_requirements_fulfilled)
      preliminary_imported_address_profiles.push_back(candidate.profile);
  }
  credit_card_save_manager_->SetPreliminarilyImportedAutofillProfile(
      preliminary_imported_address_profiles);

  bool cc_prompt_potentially_shown = ProcessExtractedCreditCard(
      submitted_form, extracted_data.extracted_credit_card,
      payment_methods_autofill_enabled,
      credit_card_save_manager_->IsCreditCardUploadEnabled());
  fetched_card_instrument_id_.reset();

  bool iban_prompt_potentially_shown = false;
  if (extracted_data.iban_import_candidate.has_value() &&
      payment_methods_autofill_enabled) {
    iban_prompt_potentially_shown =
        ProcessIbanImportCandidate(*extracted_data.iban_import_candidate);
  }

  // If a prompt for credit cards or IBANs is potentially shown, do not allow
  // for a second address profile import dialog.
  ProcessAddressProfileImportCandidates(
      extracted_data.address_profile_import_candidates,
      !cc_prompt_potentially_shown && !iban_prompt_potentially_shown);
}

bool FormDataImporter::ComplementCountry(
    AutofillProfile& profile,
    const std::string& predicted_country_code) {
  bool should_complement_country = !profile.HasRawInfo(ADDRESS_HOME_COUNTRY);
  return should_complement_country &&
         profile.SetInfoWithVerificationStatus(
             AutofillType(ADDRESS_HOME_COUNTRY),
             base::ASCIIToUTF16(predicted_country_code), app_locale_,
             VerificationStatus::kObserved);
}

bool FormDataImporter::SetPhoneNumber(
    AutofillProfile& profile,
    PhoneNumber::PhoneCombineHelper& combined_phone) {
  if (combined_phone.IsEmpty())
    return true;
  std::u16string constructed_number;
  // If the phone number only consists of a single component, the
  // `PhoneCombineHelper` won't try to parse it. This happens during `SetInfo()`
  // in this case.
  bool parsed_successfully =
      combined_phone.ParseNumber(profile, app_locale_, &constructed_number) &&
      profile.SetInfoWithVerificationStatus(PHONE_HOME_WHOLE_NUMBER,
                                            constructed_number, app_locale_,
                                            VerificationStatus::kObserved);
  autofill_metrics::LogPhoneNumberImportParsingResult(parsed_successfully);
  return parsed_successfully;
}

void FormDataImporter::RemoveInaccessibleProfileValues(
    AutofillProfile& profile) {
  const ServerFieldTypeSet inaccessible_fields =
      profile.FindInaccessibleProfileValues();
  profile.ClearFields(inaccessible_fields);
  autofill_metrics::LogRemovedSettingInaccessibleFields(
      !inaccessible_fields.empty());
  for (const ServerFieldType inaccessible_field : inaccessible_fields) {
    autofill_metrics::LogRemovedSettingInaccessibleField(inaccessible_field);
  }
}

void FormDataImporter::CacheFetchedVirtualCard(
    const std::u16string& last_four) {
  fetched_virtual_cards_.insert(last_four);
}

void FormDataImporter::SetFetchedCardInstrumentId(int64_t instrument_id) {
  fetched_card_instrument_id_ = instrument_id;
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
  credit_card_import_type_ = CreditCardImportType::kNoCard;
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_credit_card =
        ExtractCreditCard(submitted_form);
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (payment_methods_autofill_enabled) {
    extracted_form_data.iban_import_candidate = ExtractIban(submitted_form);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  size_t num_complete_address_profiles = 0;
  if (profile_autofill_enabled &&
      !base::FeatureList::IsEnabled(features::kAutofillDisableAddressImport)) {
    num_complete_address_profiles = ExtractAddressProfiles(
        submitted_form, &extracted_form_data.address_profile_import_candidates);
  }

  if (profile_autofill_enabled && payment_methods_autofill_enabled &&
      base::FeatureList::IsEnabled(features::kAutofillAssociateForms)) {
    auto origin = url::Origin::Create(submitted_form.source_url());
    FormSignature form_signature = submitted_form.form_signature();
    // If multiple complete address profiles were extracted, this most likely
    // corresponds to billing and shipping sections within the same form.
    for (size_t i = 0; i < num_complete_address_profiles; i++) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kAddressForm);
    }
    if (extracted_form_data.extracted_credit_card) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kCreditCardForm);
    }
  }

  if (!extracted_form_data.extracted_credit_card &&
      num_complete_address_profiles == 0 &&
      !extracted_form_data.iban_import_candidate) {
    personal_data_manager_->MarkObserversInsufficientFormDataForImport();
  }
  return extracted_form_data;
}

size_t FormDataImporter::ExtractAddressProfiles(
    const FormStructure& form,
    std::vector<FormDataImporter::AddressProfileImportCandidate>*
        address_profile_import_candidates) {
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
    std::map<Section, std::vector<const AutofillField*>> section_fields;
    for (const auto& field : form) {
      if (IsAddressType(field->Type())) {
        section_fields[field->section].push_back(field.get());
      }
    }

    for (const auto& [section, fields] : section_fields) {
      if (num_complete_profiles == kMaxNumAddressProfilesSaved)
        break;
      // Log the output from a section in a separate div for readability.
      LOG_AF(import_log_buffer)
          << Tag{"div"} << Attrib{"class", "profile_import_from_form_section"};
      LOG_AF(import_log_buffer)
          << LogMessage::kImportAddressProfileFromFormSection << section
          << CTag{};
      // Try to extract an address profile from the form fields of this section.
      // Only allow for a prompt if no other complete profile was found so far.
      if (ExtractAddressProfileFromSection(fields, form.source_url(),
                                           address_profile_import_candidates,
                                           &import_log_buffer)) {
        num_complete_profiles++;
      }
      // And close the div of the section import log.
      LOG_AF(import_log_buffer) << CTag{"div"};
    }
    autofill_metrics::LogAddressFormImportStatusMetric(
        num_complete_profiles == 0
            ? autofill_metrics::AddressProfileImportStatusMetric::kNoImport
            : autofill_metrics::AddressProfileImportStatusMetric::
                  kRegularImport);
  }
  LOG_AF(import_log_buffer)
      << LogMessage::kImportAddressProfileFromFormNumberOfImports
      << num_complete_profiles << CTag{};

  // Write log buffer to autofill-internals.
  LOG_AF(log_manager) << std::move(import_log_buffer);

  return num_complete_profiles;
}

bool FormDataImporter::LogAddressFormImportRequirementMetric(
    const AutofillProfile& profile,
    LogBuffer* import_log_buffer) {
  base::flat_set<autofill_metrics::AddressProfileImportRequirementMetric>
      autofill_profile_requirement_results =
          GetAutofillProfileRequirementResult(profile, import_log_buffer);

  for (const auto& requirement_result : autofill_profile_requirement_results) {
    autofill_metrics::LogAddressFormImportRequirementMetric(requirement_result);
  }

  autofill_metrics::LogAddressFormImportCountrySpecificFieldRequirementsMetric(
      autofill_profile_requirement_results.contains(
          AddressImportRequirement::kZipRequirementViolated),
      autofill_profile_requirement_results.contains(
          AddressImportRequirement::kStateRequirementViolated),
      autofill_profile_requirement_results.contains(
          AddressImportRequirement::kCityRequirementViolated),
      autofill_profile_requirement_results.contains(
          AddressImportRequirement::kLine1RequirementViolated));

  return !base::ranges::any_of(
      kMinimumAddressRequirementViolations,
      [&](AddressImportRequirement address_requirement_violation) {
        return autofill_profile_requirement_results.contains(
            address_requirement_violation);
      });
}

bool FormDataImporter::ExtractAddressProfileFromSection(
    base::span<const AutofillField* const> section_fields,
    const GURL& source_url,
    std::vector<FormDataImporter::AddressProfileImportCandidate>*
        address_profile_import_candidates,
    LogBuffer* import_log_buffer) {
  // TODO(crbug.com/1464568): Design a proper import mechanism for i18n address
  // model.
  if (base::FeatureList::IsEnabled(features::kAutofillUseI18nAddressModel)) {
    return false;
  }
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

  // Tracks if the form section contains an invalid country.
  bool has_invalid_country = false;

  // Tracks if subsequent phone number fields should be ignored,
  // since they do not belong to the first phone number in the form.
  bool ignore_phone_number_fields = false;

  // Metadata about the way we construct candidate_profile.
  ProfileImportMetadata import_metadata{.origin =
                                            url::Origin::Create(source_url)};

  // Tracks if any of the fields belongs to FormType::kAddressForm.
  bool has_address_related_fields = false;

  plus_addresses::PlusAddressService* plus_address_service =
      client_->GetPlusAddressService();
  // Go through each |form| field and attempt to constitute a valid profile.
  for (const auto* field : section_fields) {
    std::u16string value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty())
      continue;

    // When the experimental plus addresses feature is enabled, and the value is
    // a plus address, exclude it from the resulting address profile.
    if (plus_address_service &&
        plus_address_service->IsPlusAddress(base::UTF16ToUTF8(value))) {
      continue;
    }

    // When `kAutofillImportFromAutocompleteUnrecognized` is enabled, Autofill
    // imports from fields despite an unrecognized autocomplete attribute.
    if (field->ShouldSuppressSuggestionsAndFillingByDefault()) {
      if (!features::kAutofillImportFromAutocompleteUnrecognized.Get()) {
        continue;
      }
      import_metadata.num_autocomplete_unrecognized_fields++;
    }

    AutofillType field_type = field->Type();

    // Credit card fields are handled by ExtractCreditCard().
    if (field_type.group() == FieldTypeGroup::kCreditCard)
      continue;

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
    if (field_type.group() == FieldTypeGroup::kPhone &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
      if (ignore_phone_number_fields)
        continue;
      // Each phone number related type only occurs once per number. Seeing a
      // type a second time implies that it belongs to a new number. Since
      // Autofill currently supports storing only one phone number per profile,
      // ignore this and all subsequent phone number fields.
      if (types_seen.count(server_field_type)) {
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
      has_invalid_country = has_invalid_country ||
                            !candidate_profile.HasRawInfo(ADDRESS_HOME_COUNTRY);
    }

    if (FieldTypeGroupToFormType(field_type.group()) ==
        FormType::kAddressForm) {
      has_address_related_fields = true;
      if (field->parsed_autocomplete) {
        import_metadata.did_import_from_unrecognized_autocomplete_field |=
            field->parsed_autocomplete->field_type ==
            HtmlFieldType::kUnrecognized;
      }
    }
  }

  // When setting a phone number, the region is deduced from the profile's
  // country or the app locale. For the variation country code to take
  // precedence over the app locale, country code complemention needs to happen
  // before `SetPhoneNumber()`.
  const std::string predicted_country_code = GetPredictedCountryCode(
      candidate_profile, client_->GetVariationConfigCountryCode(), app_locale_,
      import_log_buffer);
  import_metadata.did_complement_country =
      ComplementCountry(candidate_profile, predicted_country_code);

  if (!SetPhoneNumber(candidate_profile, combined_phone)) {
    candidate_profile.ClearFields({PHONE_HOME_WHOLE_NUMBER});
    import_metadata.phone_import_status = PhoneImportStatus::kInvalid;
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormRemoveInvalidValue
        << "Phone number." << CTag{};
  } else if (!combined_phone.IsEmpty()) {
    import_metadata.phone_import_status = PhoneImportStatus::kValid;
  }

  // This is done prior to checking the validity of the profile, because multi-
  // step import profile merging requires the profile to be finalized. Ideally
  // we would return false here if it fails, but that breaks the metrics.
  bool finalized_import = candidate_profile.FinalizeAfterImport();

  // Reject the profile if the validation requirements are not met.
  // `IsValidLearnableProfile()` goes first to collect metrics.
  bool has_invalid_information =
      !IsValidLearnableProfile(candidate_profile, import_log_buffer) ||
      has_multiple_distinct_email_addresses || has_invalid_field_types;

  // Profiles with valid information qualify for multi-step imports.
  // This requires the profile to be finalized to apply the merging logic.
  if (finalized_import && has_address_related_fields &&
      !has_invalid_information) {
    multistep_importer_.ProcessMultiStepImport(candidate_profile,
                                               import_metadata);
  }

  // This relies on the profile's country code and must be done strictly after
  // `ComplementCountry()`.
  RemoveInaccessibleProfileValues(candidate_profile);

  // Do not import a profile if any of the requirements is violated.
  bool all_fulfilled = LogAddressFormImportRequirementMetric(
                           candidate_profile, import_log_buffer) &&
                       !has_invalid_information;

  // Collect metrics regarding the requirements for an address profile import.
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_multiple_distinct_email_addresses
          ? AddressImportRequirement::kEmailAddressUniqueRequirementViolated
          : AddressImportRequirement::kEmailAddressUniqueRequirementFulfilled);

  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_invalid_field_types
          ? AddressImportRequirement::kNoInvalidFieldTypesRequirementViolated
          : AddressImportRequirement::kNoInvalidFieldTypesRequirementFulfilled);

  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_invalid_country
          ? AddressImportRequirement::kCountryValidRequirementViolated
          : AddressImportRequirement::kCountryValidRequirementFulfilled);

  autofill_metrics::LogAddressFormImportRequirementMetric(
      all_fulfilled ? AddressImportRequirement::kOverallRequirementFulfilled
                    : AddressImportRequirement::kOverallRequirementViolated);

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
  DCHECK(!client_->IsOffTheRecord());

  AddressProfileImportCandidate import_candidate;
  import_candidate.profile = candidate_profile;
  import_candidate.url = source_url;
  import_candidate.all_requirements_fulfilled = all_fulfilled;
  import_candidate.import_metadata = import_metadata;
  address_profile_import_candidates->push_back(import_candidate);

  // Return true if a complete importable profile was found.
  return all_fulfilled;
}

bool FormDataImporter::ProcessAddressProfileImportCandidates(
    const std::vector<FormDataImporter::AddressProfileImportCandidate>&
        address_profile_import_candidates,
    bool allow_prompt) {
  int imported_profiles = 0;

  // `allow_prompt` is true if no credit card or IBAN prompt was shown. If it is
  // true, we know there is no UI currently displaying, so we can display UI to
  // import addresses. If it is false, we should not display UI to import
  // addresses due to a possible dialog or bubble conflict.
  if (allow_prompt) {
    for (const auto& candidate : address_profile_import_candidates) {
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
  for (const auto& candidate : address_profile_import_candidates) {
    // First try to import a single complete profile.
    address_profile_save_manager_->ImportProfileFromForm(
        candidate.profile, app_locale_, candidate.url,
        /*allow_only_silent_updates=*/true, candidate.import_metadata);
  }
  return false;
}

bool FormDataImporter::ProcessExtractedCreditCard(
    const FormStructure& submitted_form,
    const absl::optional<CreditCard>& extracted_credit_card,
    bool payment_methods_autofill_enabled,
    bool is_credit_card_upstream_enabled) {
  // If no card was successfully extracted from the form, return.
  if (credit_card_import_type_ == CreditCardImportType::kNoCard) {
    return false;
  }

  // If a flow where there was no interactive authentication was completed, we
  // might need to initiate the re-auth opt-in flow.
  if (auto* mandatory_reauth_manager =
          client_->GetOrCreatePaymentsMandatoryReauthManager();
      mandatory_reauth_manager &&
      mandatory_reauth_manager->ShouldOfferOptin(
          card_record_type_if_non_interactive_authentication_flow_completed_)) {
    card_record_type_if_non_interactive_authentication_flow_completed_.reset();
    mandatory_reauth_manager->StartOptInFlow();
    return true;
  }

  // If a virtual card was extracted from the form, return as we do not do
  // anything with virtual cards beyond this point.
  if (credit_card_import_type_ == CreditCardImportType::kVirtualCard) {
    return false;
  }

  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return false;
  }

  if (ShouldOfferVirtualCardEnrollment(extracted_credit_card,
                                       fetched_card_instrument_id_)) {
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *extracted_credit_card, VirtualCardEnrollmentSource::kDownstream);
    return true;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // A credit card was successfully extracted, but it's possible it is already a
  // local or server card. First, check to see if we should offer local card
  // migration in this case, as local cards could go either way.
  if (local_card_migration_manager_ &&
      local_card_migration_manager_->ShouldOfferLocalCardMigration(
          extracted_credit_card, credit_card_import_type_)) {
    local_card_migration_manager_->AttemptToOfferLocalCardMigration(
        /*is_from_settings_page=*/false);
    return true;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Local card migration will not be offered. We check to see if it is valid to
  // offer upload save or local card save, which will happen below if we do not
  // early return false in this if-statement. It will also check to see if it is
  // valid to offer CVC local or upload save.
  if (!ShouldOfferCreditCardSave(extracted_credit_card,
                                 is_credit_card_upstream_enabled)) {
    return false;
  }

  // We have a card to save; decide what type of save flow to display.
  if (is_credit_card_upstream_enabled) {
    // If the card extracted from the form is the server card, and
    // `ShouldOfferCreditCardSave` call above allowed a CVC upload save, attempt
    // to offer CVC upload save. CVC upload save should only be offered if the
    // upstream is enabled, as this implies Chrome Sync is enabled, which is a
    // requirement for any flow that involves server cards. Otherwise the users
    // will be saving a CVC to a card that is not currently autofillable or
    // present in the settings page.
    if (credit_card_import_type_ == CreditCardImportType::kServerCard) {
      credit_card_save_manager_->AttemptToOfferCvcUploadSave(
          *extracted_credit_card);
    } else {
      // Attempt to offer upload save. This block can be reached on observing
      // either a new card or one already stored locally which doesn't match an
      // existing server card. If Google Payments declines allowing upload,
      // `credit_card_save_manager_` is tasked with deciding if we should fall
      // back to local save or not.
      credit_card_save_manager_->AttemptToOfferCardUploadSave(
          submitted_form, *extracted_credit_card,
          /*uploading_local_card=*/credit_card_import_type_ ==
              CreditCardImportType::kLocalCard);
    }
    return true;
  }

  // We should offer CVC local save for local cards.
  if (credit_card_import_type_ == CreditCardImportType::kLocalCard) {
    credit_card_save_manager_->AttemptToOfferCvcLocalSave(
        *extracted_credit_card);
    return true;
  }

  // If upload save is not allowed, new cards should be saved locally.
  DCHECK(credit_card_import_type_ == CreditCardImportType::kNewCard);
  if (credit_card_save_manager_->AttemptToOfferCardLocalSave(
          *extracted_credit_card)) {
    return true;
  }

  return false;
}

bool FormDataImporter::ProcessIbanImportCandidate(
    const Iban& iban_import_candidate) {
  return iban_save_manager_->AttemptToOfferIbanLocalSave(iban_import_candidate);
}

absl::optional<CreditCard> FormDataImporter::ExtractCreditCard(
    const FormStructure& form) {
  // The candidate for credit card import. There are many ways for the candidate
  // to be rejected as indicated by the `return absl::nullopt` statements below.
  auto [candidate, form_has_duplicate_cc_type] =
      ExtractCreditCardFromForm(form);
  if (form_has_duplicate_cc_type)
    return absl::nullopt;

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
  if (!candidate.HasValidCardNumber())
    return absl::nullopt;

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
       personal_data_manager_->GetLocalCreditCards()) {
    // Make a local copy so that the data in `local_credit_cards_` isn't
    // modified directly by the UpdateFromImportedCard() call.
    CreditCard maybe_updated_card = *local_card;
    if (maybe_updated_card.UpdateFromImportedCard(candidate, app_locale_)) {
      personal_data_manager_->UpdateCreditCard(maybe_updated_card);
      credit_card_import_type_ = CreditCardImportType::kLocalCard;
      if (!maybe_updated_card.nickname().empty()) {
        // The nickname may be shown in the upload save bubble.
        candidate.SetNickname(maybe_updated_card.nickname());
      }
    }
  }

  // Return `candidate` if no server card is matched but the card in the form is
  // a valid card.
  return TryMatchingExistingServerCard(candidate);
}

absl::optional<CreditCard> FormDataImporter::TryMatchingExistingServerCard(
    const CreditCard& candidate) {
  // Used for logging purposes later if we found a matching masked server card
  // with the same last four digits, but different expiration date as
  // `candidate`, and we treat it as a new card.
  bool same_last_four_but_different_expiration_date = false;

  for (auto* server_card : personal_data_manager_->GetServerCreditCards()) {
    if (!server_card->HasSameNumberAs(candidate)) {
      continue;
    }

    // Cards with invalid expiration dates can be uploaded due to the existence
    // of the expiration date fix flow, however, since a server card with same
    // number is found, the imported card is treated as invalid card, abort
    // importing.
    if (!candidate.HasValidExpirationDate()) {
      return absl::nullopt;
    }

    if (server_card->record_type() == CreditCard::RecordType::kFullServerCard) {
      AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
          server_card->HasSameExpirationDateAs(candidate)
              ? AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED
              : AutofillMetrics::
                    FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH);
      // Return that we found a full server card with a matching card number
      // to `candidate`.
      credit_card_import_type_ = CreditCardImportType::kServerCard;
      return *server_card;
    }

    // Only return the masked server card if both the last four digits and
    // expiration date match.
    if (server_card->HasSameExpirationDateAs(candidate)) {
      AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
          AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED);

      // Return that we found a masked server card with matching last four
      // digits and copy over the user entered CVC so that future processing
      // logic check if CVC upload save should be offered.
      credit_card_import_type_ = CreditCardImportType::kServerCard;
      CreditCard server_card_with_cvc = *server_card;
      server_card_with_cvc.set_cvc(candidate.cvc());
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

absl::optional<Iban> FormDataImporter::ExtractIban(const FormStructure& form) {
  Iban candidate_iban = ExtractIbanFromForm(form);
  if (candidate_iban.value().empty())
    return absl::nullopt;

  // Sets the `kAutofillHasSeenIban` pref to true indicating that the user has
  // submitted a form with an IBAN, which indicates that the user is familiar
  // with IBANs as a concept. We set the pref so that even if the user travels
  // to a country where IBAN functionality is not typically used, they will
  // still be able to save new IBANs from the settings page using this pref.
  personal_data_manager_->SetAutofillHasSeenIban();

  return candidate_iban;
}

FormDataImporter::ExtractCreditCardFromFormResult
FormDataImporter::ExtractCreditCardFromForm(const FormStructure& form) {
  ExtractCreditCardFromFormResult result;

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

    ServerFieldType server_field_type = field_type.GetStorableType();
    result.has_duplicate_credit_card_field_type |=
        types_seen.contains(server_field_type);
    types_seen.insert(server_field_type);

    // If |field| is an HTML5 month input, handle it as a special case.
    if (field->form_control_type == FormControlType::kInputMonth) {
      DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR, server_field_type);
      result.card.SetInfoForMonthInputType(value);
      continue;
    }

    // CreditCard handles storing the |value| according to |field_type|.
    bool saved = result.card.SetInfo(field_type, value, app_locale_);

    // Saving with the option text (here |value|) may fail for the expiration
    // month. Attempt to save with the option value. First find the index of the
    // option text in the select options and try the corresponding value.
    if (!saved && server_field_type == CREDIT_CARD_EXP_MONTH) {
      for (const SelectOption& option : field->options) {
        if (value == option.content) {
          result.card.SetInfo(field_type, option.value, app_locale_);
          break;
        }
      }
    }
  }

  return result;
}

Iban FormDataImporter::ExtractIbanFromForm(const FormStructure& form) {
  // Creates an IBAN candidate with `kUnknown` record type as it is currently
  // unknown if this IBAN already exists locally or on the server.
  Iban candidate_iban;

  for (const auto& field : form) {
    if (!field->IsFieldFillable() || field->value.empty()) {
      continue;
    }

    AutofillType field_type = field->Type();
    if (field_type.GetStorableType() == IBAN_VALUE &&
        Iban::IsValid(field->value)) {
      candidate_iban.SetInfo(field_type, field->value, app_locale_);
      break;
    }
  }

  return candidate_iban;
}

// TODO(crbug.com/1450749): Move ShouldOfferCreditCardSave to
// credit_card_save_manger and combine all card and CVC save logic to
// ProceedWithSavingIfApplicable function.
bool FormDataImporter::ShouldOfferCreditCardSave(
    const absl::optional<CreditCard>& extracted_credit_card,
    bool is_credit_card_upstream_enabled) {
  // If we have an invalid card in the form, a duplicate field type, or we have
  // entered a virtual card, `extracted_credit_card` is nullptr and thus we do
  // not want to offer upload save or local card save.
  if (!extracted_credit_card) {
    return false;
  }

  // Check if CVC local or upload save should be offered.
  if (credit_card_save_manager_->ShouldOfferCvcSave(
          *extracted_credit_card, credit_card_import_type_,
          is_credit_card_upstream_enabled)) {
    return true;
  }

  // We do not want to offer upload save or local card save for server cards.
  if (credit_card_import_type_ == CreditCardImportType::kServerCard) {
    return false;
  }

  // Credit card upload save is not offered for local cards if upstream is
  // disabled. Local save is not offered for local cards if the card is already
  // saved as a local card.
  if (!is_credit_card_upstream_enabled &&
      credit_card_import_type_ == CreditCardImportType::kLocalCard) {
    return false;
  }

  // We know `extracted_credit_card` is either a new card, or a local
  // card with upload enabled.
  return true;
}

void FormDataImporter::OnPersonalDataChanged() {
  // `personal_data_manager_` cannot be null, because the callback cannot be
  // registered otherwise.
  DCHECK(personal_data_manager_);
  multistep_importer_.OnPersonalDataChanged(*personal_data_manager_);
}

void FormDataImporter::OnBrowsingHistoryCleared(
    const history::DeletionInfo& deletion_info) {
  multistep_importer_.OnBrowsingHistoryCleared(deletion_info);
  form_associator_.OnBrowsingHistoryCleared(deletion_info);
}

void FormDataImporter::
    SetCardRecordTypeIfNonInteractiveAuthenticationFlowCompleted(
        absl::optional<CreditCard::RecordType>
            card_record_type_if_non_interactive_authentication_flow_completed) {
  card_record_type_if_non_interactive_authentication_flow_completed_ =
      card_record_type_if_non_interactive_authentication_flow_completed;
}

absl::optional<CreditCard::RecordType>
FormDataImporter::GetCardRecordTypeIfNonInteractiveAuthenticationFlowCompleted()
    const {
  return card_record_type_if_non_interactive_authentication_flow_completed_;
}

}  // namespace autofill
