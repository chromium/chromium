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

#include "base/check_deref.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/address_profile_save_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_i18n_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/iban_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/profile_requirement_utils.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

namespace {

using AddressImportRequirement =
    autofill_metrics::AddressProfileImportRequirementMetric;

// Return true if the |field_type| and |value| are valid within the context
// of importing a form.
bool IsValidFieldTypeAndValue(
    const base::flat_map<FieldType, std::u16string>& observed_types,
    FieldType field_type,
    const std::u16string& value,
    LogBuffer* import_log_buffer) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for:
  // - EMAIL_ADDRESS because it is common to see second 'confirm email address'
  // field;
  // - phone number components because a form might request several phone
  // numbers.
  // TODO(crbug.com/40735892) Clean up when launched.
  FieldTypeGroup field_type_group = GroupTypeOfFieldType(field_type);
  if (observed_types.contains(field_type) && field_type != EMAIL_ADDRESS &&
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
    const std::optional<CreditCard>& extracted_credit_card,
    std::optional<int64_t> fetched_card_instrument_id) {
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

bool HasSynthesizedTypes(
    const base::flat_map<FieldType, std::u16string>& observed_field_values,
    AddressCountryCode country_code) {
  return std::ranges::any_of(observed_field_values, [country_code](
                                                        const auto& entry) {
    return i18n_model_definition::IsSynthesizedType(entry.first, country_code);
  });
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
                                   history::HistoryService* history_service,
                                   const std::string& app_locale)
    : client_(CHECK_DEREF(client)),
      credit_card_save_manager_(
          std::make_unique<CreditCardSaveManager>(client, app_locale)),
      address_profile_save_manager_(
          std::make_unique<AddressProfileSaveManager>(client)),
#if !BUILDFLAG(IS_IOS)
      iban_save_manager_(std::make_unique<IbanSaveManager>(client)),
#endif  // !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      local_card_migration_manager_(
          std::make_unique<LocalCardMigrationManager>(client, app_locale)),
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      app_locale_(app_locale),
      multistep_importer_(app_locale,
                          client_->GetVariationConfigCountryCode()) {
  address_data_manager_observation_.Observe(&address_data_manager());
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
}

FormDataImporter::~FormDataImporter() = default;

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
      credit_card_save_manager_->IsCreditCardUploadEnabled());
  fetched_card_instrument_id_.reset();

  bool iban_prompt_potentially_shown = false;
  if (extracted_data.extracted_iban.has_value() &&
      payment_methods_autofill_enabled) {
    iban_prompt_potentially_shown =
        ProcessIbanImportCandidate(*extracted_data.extracted_iban);
  }

  // If a prompt for credit cards or IBANs is potentially shown, do not allow
  // for a second address profile import dialog.
  ProcessAddressProfileImportCandidates(
      extracted_data.address_profile_import_candidates,
      !cc_prompt_potentially_shown && !iban_prompt_potentially_shown);
}

bool FormDataImporter::ComplementCountry(AutofillProfile& profile,
                                         LogBuffer* import_log_buffer) {
  if (profile.HasRawInfo(ADDRESS_HOME_COUNTRY)) {
    return false;
  }
  const std::string fallback =
      address_data_manager().GetDefaultCountryCodeForNewAddress().value();
  if (import_log_buffer) {
    *import_log_buffer
        << LogMessage::kImportAddressProfileComplementedCountryCode << fallback
        << CTag{};
  }
  return profile.SetInfoWithVerificationStatus(
      ADDRESS_HOME_COUNTRY, base::ASCIIToUTF16(fallback), app_locale_,
      VerificationStatus::kObserved);
}

bool FormDataImporter::SetPhoneNumber(
    AutofillProfile& profile,
    const PhoneNumber::PhoneCombineHelper& combined_phone) {
  if (combined_phone.IsEmpty())
    return true;

  bool parsed_successfully = PhoneNumber::ImportPhoneNumberToProfile(
      combined_phone, app_locale_, profile);
  autofill_metrics::LogPhoneNumberImportParsingResult(parsed_successfully);
  return parsed_successfully;
}

void FormDataImporter::RemoveInaccessibleProfileValues(
    AutofillProfile& profile) {
  const FieldTypeSet inaccessible_fields =
      profile.FindInaccessibleProfileValues();
  profile.ClearFields(inaccessible_fields);
  autofill_metrics::LogRemovedSettingInaccessibleFields(
      !inaccessible_fields.empty());
  for (const FieldType inaccessible_field : inaccessible_fields) {
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

#if !BUILDFLAG(IS_IOS)
  if (payment_methods_autofill_enabled) {
    extracted_form_data.extracted_iban = ExtractIban(submitted_form);
  }
#endif  // !BUILDFLAG(IS_IOS)

  size_t num_complete_address_profiles = 0;
  if (profile_autofill_enabled &&
      !base::FeatureList::IsEnabled(features::kAutofillDisableAddressImport)) {
    num_complete_address_profiles = ExtractAddressProfiles(
        submitted_form, &extracted_form_data.address_profile_import_candidates);
  }

  if (profile_autofill_enabled && payment_methods_autofill_enabled) {
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
      if (IsAddressType(field->Type().GetStorableType())) {
        section_fields[field->section()].push_back(field.get());
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

AutofillProfile FormDataImporter::ConstructProfileFromObservedValues(
    const base::flat_map<FieldType, std::u16string>& observed_values,
    LogBuffer* import_log_buffer,
    ProfileImportMetadata& import_metadata) {
  AutofillProfile candidate_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);

  auto country_it = observed_values.find(ADDRESS_HOME_COUNTRY);
  if (country_it != observed_values.end()) {
    // Try setting the collected country value into the profile and report
    // invalid country if the operation failed.
    candidate_profile.SetInfoWithVerificationStatus(
        ADDRESS_HOME_COUNTRY, country_it->second, app_locale_,
        VerificationStatus::kObserved);

    // Track the validity of the entered country for metrics.
    import_metadata.observed_invalid_country =
        !candidate_profile.HasRawInfo(ADDRESS_HOME_COUNTRY);
  }

  // When setting a phone number, the region is deduced from the profile's
  // country or the app locale. For the variation country code to take
  // precedence over the app locale, country code complemention needs to happen
  // before `SetPhoneNumber()`.
  import_metadata.did_complement_country =
      ComplementCountry(candidate_profile, import_log_buffer);

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper combined_phone;

  // Populate the profile with the collected values. Note that this is after the
  // profile's country has been set to make sure the correct address
  // representation is used.
  for (const auto& [type, value] : observed_values) {
    // The profile country has already been established by this point. It's
    // ignored here to avoid re-setting up a potentially invalid country that
    // was present in the form.
    if (type == ADDRESS_HOME_COUNTRY) {
      continue;
    }
    if (GroupTypeOfFieldType(type) == FieldTypeGroup::kPhone) {
      // We need to store phone data in the variables, before building the whole
      // number at the end.
      combined_phone.SetInfo(type, value);
    } else {
      candidate_profile.SetInfoWithVerificationStatus(
          type, value, app_locale_, VerificationStatus::kObserved);
    }
  }

  if (!SetPhoneNumber(candidate_profile, combined_phone)) {
    candidate_profile.ClearFields({PHONE_HOME_WHOLE_NUMBER});
    import_metadata.phone_import_status = PhoneImportStatus::kInvalid;
    LOG_AF(import_log_buffer)
        << LogMessage::kImportAddressProfileFromFormRemoveInvalidValue
        << "Phone number." << CTag{};
  } else if (!combined_phone.IsEmpty()) {
    import_metadata.phone_import_status = PhoneImportStatus::kValid;
  }
  return candidate_profile;
}

base::flat_map<FieldType, std::u16string>
FormDataImporter::GetAddressObservedFieldValues(
    base::span<const AutofillField* const> section_fields,
    ProfileImportMetadata& import_metadata,
    LogBuffer* import_log_buffer,
    bool& has_invalid_field_types,
    bool& has_multiple_distinct_email_addresses,
    bool& has_address_related_fields) const {
  AutofillPlusAddressDelegate* plus_address_delegate =
      client_->GetPlusAddressDelegate();
  base::flat_map<FieldType, std::u16string> observed_field_values;

  // Tracks if subsequent phone number fields should be ignored,
  // since they do not belong to the first phone number in the form.
  bool ignore_phone_number_fields = false;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const AutofillField* const field : section_fields) {
    std::u16string value = field->value_for_import();
    base::TrimWhitespace(value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    // If the field was filled with a fallback type, skip it in order to not
    // introduce noise to the map's data, as this would add an entry for
    // field type X with a value retrieved from another field type Y.
    if (field->WasAutofilledWithFallback()) {
      continue;
    }
    // When the experimental plus addresses feature is enabled, and the value is
    // a plus address, exclude it from the resulting address profile.
    if (plus_address_delegate &&
        plus_address_delegate->IsPlusAddress(base::UTF16ToUTF8(value))) {
      continue;
    }
    // Don't import from ac=unrecognized fields.
    if (field->ShouldSuppressSuggestionsAndFillingByDefault() &&
        !base::FeatureList::IsEnabled(
            features::kAutofillImportFromAutocompleteUnrecognized)) {
      continue;
    }

    FieldType field_type = field->Type().GetStorableType();
    // Only address types are relevant in this function, other types are treated
    // in different flows.
    if (!IsAddressType(field_type)) {
      continue;
    }
    has_address_related_fields = true;

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    if (field_type == EMAIL_ADDRESS) {
      auto email_it = observed_field_values.find(EMAIL_ADDRESS);
      if (email_it != observed_field_values.end() &&
          email_it->second != value) {
        LOG_AF(import_log_buffer)
            << LogMessage::kImportAddressProfileFromFormFailed
            << "Multiple different email addresses present." << CTag{};
        has_multiple_distinct_email_addresses = true;
      }
    }
    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(observed_field_values, field_type, value,
                                  import_log_buffer)) {
      has_invalid_field_types = true;
    }
    // Found phone number component field.
    // TODO(crbug.com/40735892) Remove feature check when launched.
    if (GroupTypeOfFieldType(field_type) == FieldTypeGroup::kPhone &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableImportWhenMultiplePhoneNumbers)) {
      if (ignore_phone_number_fields) {
        continue;
      }
      // Each phone number related type only occurs once per number. Seeing a
      // type a second time implies that it belongs to a new number. Since
      // Autofill currently supports storing only one phone number per profile,
      // ignore this and all subsequent phone number fields.
      if (observed_field_values.contains(field_type)) {
        ignore_phone_number_fields = true;
        continue;
      }
    }
    observed_field_values.insert_or_assign(field_type, value);
    // The `autofill_source_profile_guid()` is not reset when a field is
    // manually edited or filled with non-address information later.
    import_metadata.filled_types_to_autofill_guid.insert_or_assign(
        field_type, field->is_autofilled() &&
                            field->filling_product() == FillingProduct::kAddress
                        ? field->autofill_source_profile_guid()
                        : std::nullopt);

    if (field->parsed_autocomplete()) {
      import_metadata.did_import_from_unrecognized_autocomplete_field |=
          field->parsed_autocomplete()->field_type ==
          HtmlFieldType::kUnrecognized;
    }
  }
  return observed_field_values;
}

bool FormDataImporter::ExtractAddressProfileFromSection(
    base::span<const AutofillField* const> section_fields,
    const GURL& source_url,
    std::vector<FormDataImporter::AddressProfileImportCandidate>*
        address_profile_import_candidates,
    LogBuffer* import_log_buffer) {
  // Tracks if the form section contains multiple distinct email addresses.
  bool has_multiple_distinct_email_addresses = false;

  // Tracks if the form section contains an invalid types.
  bool has_invalid_field_types = false;

  // Metadata about the way we construct candidate_profile.
  ProfileImportMetadata import_metadata;
  import_metadata.origin = url::Origin::Create(source_url);

  // Tracks if any of the fields belongs to FormType::kAddressForm.
  bool has_address_related_fields = false;

  // Stores the values collected for each related `FieldType`. Used as
  // well to detect and discard address forms with multiple fields of the same
  // type.
  base::flat_map<FieldType, std::u16string> observed_field_values =
      GetAddressObservedFieldValues(section_fields, import_metadata,
                                    import_log_buffer, has_invalid_field_types,
                                    has_multiple_distinct_email_addresses,
                                    has_address_related_fields);

  // The candidate for profile import.
  AutofillProfile candidate_profile = ConstructProfileFromObservedValues(
      observed_field_values, import_log_buffer, import_metadata);

  // After ensuring the correct country is set on the profile, we can search for
  // any synthesized nodes. If any of these exist, we'll exclude the profile
  // from the import process
  bool has_synthesized_types = HasSynthesizedTypes(
      observed_field_values, candidate_profile.GetAddressCountryCode());

  // This is done prior to checking the validity of the profile, because multi-
  // step import profile merging requires the profile to be finalized. Ideally
  // we would return false here if it fails, but that breaks the metrics.
  bool finalized_import = candidate_profile.FinalizeAfterImport();

  // Reject the profile if the validation requirements are not met.
  // `ValidateNonEmptyValues()` goes first to collect metrics.
  bool has_invalid_information =
      !ValidateNonEmptyValues(candidate_profile, import_log_buffer) ||
      has_multiple_distinct_email_addresses || has_invalid_field_types ||
      has_synthesized_types;

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
  // `IsMinimumAddress()` goes first, since it logs to autofill-internals.
  bool all_fulfilled = IsMinimumAddress(candidate_profile, import_log_buffer) &&
                       !has_invalid_information;

  // Collect metrics regarding the requirements for an address profile import.
  autofill_metrics::LogAddressFormImportRequirementMetric(candidate_profile);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_multiple_distinct_email_addresses
          ? AddressImportRequirement::kEmailAddressUniqueRequirementViolated
          : AddressImportRequirement::kEmailAddressUniqueRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_invalid_field_types
          ? AddressImportRequirement::kNoInvalidFieldTypesRequirementViolated
          : AddressImportRequirement::kNoInvalidFieldTypesRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      has_synthesized_types
          ? AddressImportRequirement::kNoSythesizedTypesRequirementViolated
          : AddressImportRequirement::kNoSythesizedTypesRequirementFulfilled);
  autofill_metrics::LogAddressFormImportRequirementMetric(
      import_metadata.observed_invalid_country
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
    const std::optional<CreditCard>& extracted_credit_card,
    bool is_credit_card_upstream_enabled) {
  // If no card was successfully extracted from the form, return.
  if (credit_card_import_type_ == CreditCardImportType::kNoCard) {
    return false;
  }

  // If a flow without interactive authentication was completed and the user
  // didn't update the result that was filled into the form, re-auth opt-in flow
  // might be offered.
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
    client_->GetPaymentsAutofillClient()
        ->GetVirtualCardEnrollmentManager()
        ->InitVirtualCardEnroll(*extracted_credit_card,
                                VirtualCardEnrollmentSource::kDownstream);
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

  // Proceed with card or CVC saving if applicable.
  return extracted_credit_card &&
         credit_card_save_manager_->ProceedWithSavingIfApplicable(
             submitted_form, *extracted_credit_card, credit_card_import_type_,
             is_credit_card_upstream_enabled);
}

bool FormDataImporter::ProcessIbanImportCandidate(Iban& extracted_iban) {
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

std::optional<CreditCard> FormDataImporter::ExtractCreditCard(
    const FormStructure& form) {
  // The candidate for credit card import. There are many ways for the candidate
  // to be rejected as indicated by the `return std::nullopt` statements below.
  auto [candidate, form_has_duplicate_cc_type] =
      ExtractCreditCardFromForm(form);
  if (form_has_duplicate_cc_type)
    return std::nullopt;

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
    return std::nullopt;

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
    if (maybe_updated_card.UpdateFromImportedCard(candidate, app_locale_)) {
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
  return TryMatchingExistingServerCard(candidate);
}

std::optional<CreditCard> FormDataImporter::TryMatchingExistingServerCard(
    const CreditCard& candidate) {
  // Used for logging purposes later if we found a matching masked server card
  // with the same last four digits, but different expiration date as
  // `candidate`, and we treat it as a new card.
  bool same_last_four_but_different_expiration_date = false;

  for (auto* server_card : payments_data_manager().GetServerCreditCards()) {
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
      if (credit_card_import_type_ == CreditCardImportType::kLocalCard) {
        credit_card_import_type_ =
            CreditCardImportType::kDuplicateLocalServerCard;
      } else {
        credit_card_import_type_ = CreditCardImportType::kServerCard;
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

std::optional<Iban> FormDataImporter::ExtractIban(const FormStructure& form) {
  Iban candidate_iban = ExtractIbanFromForm(form);
  if (candidate_iban.value().empty())
    return std::nullopt;

  // Sets the `kAutofillHasSeenIban` pref to true indicating that the user has
  // submitted a form with an IBAN, which indicates that the user is familiar
  // with IBANs as a concept. We set the pref so that even if the user travels
  // to a country where IBAN functionality is not typically used, they will
  // still be able to save new IBANs from the settings page using this pref.
  payments_data_manager().SetAutofillHasSeenIban();

  return candidate_iban;
}

FormDataImporter::ExtractCreditCardFromFormResult
FormDataImporter::ExtractCreditCardFromForm(const FormStructure& form) {
  // Populated by the lambdas below.
  ExtractCreditCardFromFormResult result;

  // Populates `result` from `field` if it's a credit card field.
  // For example, if `field` contains credit card number, this sets the number
  // of `result.card` to the `field`'s value.
  auto extract_if_credit_card_field = [&result, &app_locale = app_locale_](
                                          const AutofillField& field) {
    std::u16string value = [&field] {
      if (field.Type().GetStorableType() == FieldType::CREDIT_CARD_NUMBER) {
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
    if (!field.IsFieldFillable() || value.empty() ||
        field.Type().group() != FieldTypeGroup::kCreditCard) {
      return;
    }
    std::u16string old_value = result.card.GetInfo(field.Type(), app_locale);
    if (field.form_control_type() == FormControlType::kInputMonth) {
      // If |field| is an HTML5 month input, handle it as a special case.
      DCHECK_EQ(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                field.Type().GetStorableType());
      result.card.SetInfoForMonthInputType(value);
    } else {
      bool saved = result.card.SetInfo(field.Type(), value, app_locale);
      if (!saved && field.IsSelectElement()) {
        // Saving with the option text (here `value`) may fail for the
        // expiration month. Attempt to save with the option value. First find
        // the index of the option text in the select options and try the
        // corresponding value.
        if (auto it =
                base::ranges::find(field.options(), value, &SelectOption::text);
            it != field.options().end()) {
          result.card.SetInfo(field.Type(), it->value, app_locale);
        }
      }
    }
    std::u16string new_value = result.card.GetInfo(field.Type(), app_locale);
    result.has_duplicate_credit_card_field_type |=
        !old_value.empty() && old_value != new_value;
  };

  // Populates `result` from `fields` that satisfy `pred`, and erases those
  // fields. Afterwards, it also erases all remaining fields whose type is now
  // present in `result.card`.
  // For example, if a `CREDIT_CARD_NAME_FULL` field matches `pred`, this
  // function sets the credit card first, last, and full name and erases
  // all `fields` of type `CREDIT_CARD_NAME_{FULL,FIRST,LAST}`.
  auto extract_data_and_remove_field_if =
      [&result, &extract_if_credit_card_field, &app_locale = app_locale_](
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
  extract_data_and_remove_field_if(fields, &AutofillField::is_user_edited);
  extract_data_and_remove_field_if(fields, &AutofillField::is_autofilled);
  extract_data_and_remove_field_if(fields, [](const auto&) { return true; });
  return result;
}

Iban FormDataImporter::ExtractIbanFromForm(const FormStructure& form) {
  // Creates an IBAN candidate with `kUnknown` record type as it is currently
  // unknown if this IBAN already exists locally or on the server.
  Iban candidate_iban;
  for (const auto& field : form) {
    const std::u16string& value = field->value_for_import();
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    FieldType field_type = field->Type().GetStorableType();
    if (field_type == IBAN_VALUE && Iban::IsValid(value)) {
      candidate_iban.SetInfo(IBAN_VALUE, value, app_locale_);
      break;
    }
  }
  return candidate_iban;
}

// TODO(crbug.com/40270301): Move ShouldOfferCreditCardSave to
// credit_card_save_manger and combine all card and CVC save logic to
// ProceedWithSavingIfApplicable function.
bool FormDataImporter::ShouldOfferCreditCardSave(
    const std::optional<CreditCard>& extracted_credit_card,
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

void FormDataImporter::OnAddressDataChanged() {
  multistep_importer_.OnAddressDataChanged(address_data_manager());
}

void FormDataImporter::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  multistep_importer_.OnBrowsingHistoryCleared(deletion_info);
  form_associator_.OnBrowsingHistoryCleared(deletion_info);
}

void FormDataImporter::
    SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
        std::optional<NonInteractivePaymentMethodType>
            payment_method_type_if_non_interactive_authentication_flow_completed) {
  payment_method_type_if_non_interactive_authentication_flow_completed_ =
      payment_method_type_if_non_interactive_authentication_flow_completed;
}

AddressDataManager& FormDataImporter::address_data_manager() {
  return client_->GetPersonalDataManager()->address_data_manager();
}

PaymentsDataManager& FormDataImporter::payments_data_manager() {
  return client_->GetPersonalDataManager()->payments_data_manager();
}

}  // namespace autofill
