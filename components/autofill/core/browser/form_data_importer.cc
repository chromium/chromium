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
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/profile_import_metrics.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
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
  auto field_type_group = AutofillType(field_type).group();
  if (types_seen.count(field_type) && field_type != EMAIL_ADDRESS &&
      (!base::FeatureList::IsEnabled(
           features::kAutofillEnableImportWhenMultiplePhoneNumbers) ||
       (field_type_group != FieldTypeGroup::kPhoneBilling &&
        field_type_group != FieldTypeGroup::kPhoneHome))) {
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

// |credit_card_import_candidate| refers to the credit card that was most
// recently submitted and |fetched_card_instrument_id| refers to the instrument
// id of the most recently downstreamed (fetched from the server) credit card.
// These need to match to offer virtual card enrollment for the
// |credit_card_import_candidate|.
bool ShouldOfferVirtualCardEnrollment(
    const absl::optional<CreditCard>& credit_card_import_candidate,
    absl::optional<int64_t> fetched_card_instrument_id) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableUpdateVirtualCardEnrollment)) {
    return false;
  }

  if (!credit_card_import_candidate)
    return false;

  if (credit_card_import_candidate->virtual_card_enrollment_state() !=
      CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE) {
    return false;
  }

  if (!fetched_card_instrument_id.has_value() ||
      credit_card_import_candidate->instrument_id() !=
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
          base::FeatureList::IsEnabled(features::kAutofillFillIbanFields)
              ? std::make_unique<IBANSaveManager>(client)
              : nullptr),
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

  bool cc_prompt_potentially_shown = ProcessCreditCardImportCandidate(
      submitted_form, extracted_data.credit_card_import_candidate,
      extracted_data.extracted_upi_id, payment_methods_autofill_enabled,
      credit_card_save_manager_->IsCreditCardUploadEnabled());
  fetched_card_instrument_id_.reset();

  bool iban_prompt_potentially_shown = false;
  if (extracted_data.iban_import_candidate.has_value() &&
      payment_methods_autofill_enabled) {
    iban_prompt_potentially_shown =
        ProcessIBANImportCandidate(*extracted_data.iban_import_candidate);
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
  //   `credit_card_import_candidate` contained in `extracted_form_data` with an
  //   extracted card.
  // - `ExtractAddressProfiles()` collects all importable
  // profiles, but currently
  //   at most one import prompt is shown.
  // Reset `credit_card_import_type_` every time we extract
  // data from form no matter whether `ExtractCreditCard()` is
  // called or not.
  credit_card_import_type_ = CreditCardImportType::kNoCard;
  if (payment_methods_autofill_enabled) {
    extracted_form_data.credit_card_import_candidate =
        ExtractCreditCard(submitted_form);
    extracted_form_data.extracted_upi_id = ExtractUpiId(submitted_form);
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(features::kAutofillFillIbanFields) &&
      payment_methods_autofill_enabled) {
    extracted_form_data.iban_import_candidate = ExtractIBAN(submitted_form);
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
    if (extracted_form_data.credit_card_import_candidate) {
      form_associator_.TrackFormAssociations(
          origin, form_signature, FormAssociator::FormType::kCreditCardForm);
    }
  }

  if (!extracted_form_data.credit_card_import_candidate &&
      !extracted_form_data.extracted_upi_id &&
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

bool FormDataImporter::ExtractAddressProfileFromSection(
    base::span<const AutofillField* const> section_fields,
    const GURL& source_url,
    std::vector<FormDataImporter::AddressProfileImportCandidate>*
        address_profile_import_candidates,
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

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const auto* field : section_fields) {
    std::u16string value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty())
      continue;

    // When `kAutofillImportFromAutocompleteUnrecognized` is enabled, Autofill
    // imports from fields despite an unrecognized autocomplete attribute.
    if (field->HasPredictionDespiteUnrecognizedAutocompleteAttribute()) {
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
    if ((field_type.group() == FieldTypeGroup::kPhoneBilling ||
         field_type.group() == FieldTypeGroup::kPhoneHome) &&
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

  const std::string variation_country_code =
      client_->GetVariationConfigCountryCode();
  std::string predicted_country_code =
      GetPredictedCountryCode(candidate_profile, variation_country_code,
                              app_locale_, import_log_buffer);

  // When setting a phone number, the region is deduced from the profile's
  // country or the app locale. For the `variation_country_code` to take
  // precedence over the app locale, country code complemention needs to happen
  // before `SetPhoneNumber()`.
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
    // If `candidate_profile` was merged with a profile containing
    // (non-complemented) country information, the country might have changed.
    predicted_country_code =
        GetPredictedCountryCode(candidate_profile, variation_country_code,
                                app_locale_, /*import_log_buffer=*/nullptr);
  }

  // This relies on the profile's country code and must be done strictly after
  // `ComplementCountry()`.
  RemoveInaccessibleProfileValues(candidate_profile);

  // Do not import a profile if any of the requirements is violated.
  // |IsMinimumAddress()| goes first to collect metrics.
  bool all_fulfilled =
      IsMinimumAddress(candidate_profile, predicted_country_code, app_locale_,
                       import_log_buffer, /*collect_metrics=*/true) &&
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
  DCHECK(!personal_data_manager_->IsOffTheRecord());

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

bool FormDataImporter::ProcessCreditCardImportCandidate(
    const FormStructure& submitted_form,
    const absl::optional<CreditCard>& credit_card_import_candidate,
    const absl::optional<std::string>& extracted_upi_id,
    bool payment_methods_autofill_enabled,
    bool is_credit_card_upstream_enabled) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (extracted_upi_id && payment_methods_autofill_enabled &&
      base::FeatureList::IsEnabled(features::kAutofillSaveAndFillVPA)) {
    upi_vpa_save_manager_->OfferLocalSave(*extracted_upi_id);
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // If no card was successfully extracted from the form, return.
  if (credit_card_import_type_ == CreditCardImportType::kNoCard) {
    return false;
  }
  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return false;
  }

  if (ShouldOfferVirtualCardEnrollment(credit_card_import_candidate,
                                       fetched_card_instrument_id_)) {
    virtual_card_enrollment_manager_->InitVirtualCardEnroll(
        *credit_card_import_candidate,
        VirtualCardEnrollmentSource::kDownstream);
    return true;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // A credit card was successfully extracted, but it's possible it is already a
  // local or server card. First, check to see if we should offer local card
  // migration in this case, as local cards could go either way.
  if (local_card_migration_manager_ &&
      local_card_migration_manager_->ShouldOfferLocalCardMigration(
          credit_card_import_candidate, credit_card_import_type_)) {
    local_card_migration_manager_->AttemptToOfferLocalCardMigration(
        /*is_from_settings_page=*/false);
    return true;
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Local card migration will not be offered. We check to see if it is valid to
  // offer upload save or local card save, which will happen below if we do not
  // early return false in this if-statement.
  if (!ShouldOfferUploadCardOrLocalCardSave(credit_card_import_candidate,
                                            is_credit_card_upstream_enabled)) {
    return false;
  }

  // We have a card to save; decide what type of save flow to display.
  if (is_credit_card_upstream_enabled) {
    // Attempt to offer upload save. Because we pass
    // `credit_card_upstream_enabled` to ExtractFormImportCandidates, this block
    // can be reached on observing either a new card or one already stored
    // locally which doesn't match an existing server card. If Google Payments
    // declines allowing upload, `credit_card_save_manager_` is tasked with
    // deciding if we should fall back to local save or not.
    DCHECK(credit_card_import_type_ == CreditCardImportType::kLocalCard ||
           credit_card_import_type_ == CreditCardImportType::kNewCard);
    credit_card_save_manager_->AttemptToOfferCardUploadSave(
        submitted_form, from_dynamic_change_form_, has_non_focusable_field_,
        *credit_card_import_candidate,
        /*uploading_local_card=*/credit_card_import_type_ ==
            CreditCardImportType::kLocalCard);
    return true;
  };
  // If upload save is not allowed, new cards should be saved locally.
  DCHECK(credit_card_import_type_ == CreditCardImportType::kNewCard);
  if (credit_card_save_manager_->AttemptToOfferCardLocalSave(
          from_dynamic_change_form_, has_non_focusable_field_,
          *credit_card_import_candidate)) {
    return true;
  }

  return false;
}

bool FormDataImporter::ProcessIBANImportCandidate(
    const IBAN& iban_import_candidate) {
  if (!iban_save_manager_)
    return false;

  return iban_save_manager_->AttemptToOfferIBANLocalSave(iban_import_candidate);
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

  // If the imported card is a known virtual card, abort importing.
  if (fetched_virtual_cards_.contains(candidate.LastFourDigits()))
    return absl::nullopt;

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

  // Attempt to find a matching server card. If such a server card exists,
  // return it (rather than the extracted card) because we want the server to be
  // the source of truth.
  auto is_matching_server_card = [&cand = candidate](const CreditCard* card) {
    return (card->record_type() == CreditCard::MASKED_SERVER_CARD &&
            card->LastFourDigits() == cand.LastFourDigits()) ||
           (card->record_type() == CreditCard::FULL_SERVER_CARD &&
            cand.HasSameNumberAs(*card));
  };
  auto find_matching_server_card = [&]() {
    const auto& server_cards = personal_data_manager_->GetServerCreditCards();
    const auto it =
        base::ranges::find_if(server_cards, is_matching_server_card);
    return it != server_cards.end() ? absl::optional<CreditCard>(**it)
                                    : absl::nullopt;
  };
  absl::optional<CreditCard> server_card = find_matching_server_card();
  if (!server_card)
    return candidate;

  if (candidate.expiration_month() == 0 || candidate.expiration_year() == 0)
    return absl::nullopt;

  credit_card_import_type_ = CreditCardImportType::kServerCard;

  if (candidate.expiration_month() == server_card->expiration_month() &&
      candidate.expiration_year() == server_card->expiration_year()) {
    AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
        server_card->record_type() == CreditCard::FULL_SERVER_CARD
            ? AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED
            : AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED);
  } else {
    AutofillMetrics::LogSubmittedServerCardExpirationStatusMetric(
        server_card->record_type() == CreditCard::FULL_SERVER_CARD
            ? AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH
            : AutofillMetrics::
                  MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH);
  }

  return server_card;
}

absl::optional<IBAN> FormDataImporter::ExtractIBAN(const FormStructure& form) {
  IBAN candidate_iban = ExtractIBANFromForm(form);
  if (candidate_iban.value().empty())
    return absl::nullopt;

  // Sets the `kAutofillHasSeenIban` pref to true indicating that the user has
  // submitted a form with an IBAN, which indicates that the user is familiar
  // with IBANs as a concept. We set the pref so that even if the user travels
  // to a country where IBAN functionality is not typically used, they will
  // still be able to save new IBANs from the settings page using this pref.
  personal_data_manager_->SetAutofillHasSeenIban();

  bool found_existing_local_iban = base::ranges::any_of(
      personal_data_manager_->GetLocalIBANs(), [&](const auto& iban) {
        return iban->value() == candidate_iban.value();
      });

  if (found_existing_local_iban) {
    return absl::nullopt;
  }

  // Only offer to save new IBAN. Users can go to the payment methods settings
  // page to update existing IBANs if desired.
  return candidate_iban;
}

FormDataImporter::ExtractCreditCardFromFormResult
FormDataImporter::ExtractCreditCardFromForm(const FormStructure& form) {
  has_non_focusable_field_ = false;
  from_dynamic_change_form_ = false;

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

    if (form.value_from_dynamic_change_form())
      from_dynamic_change_form_ = true;
    if (!field->is_focusable)
      has_non_focusable_field_ = true;

    ServerFieldType server_field_type = field_type.GetStorableType();
    result.has_duplicate_credit_card_field_type |=
        types_seen.contains(server_field_type);
    types_seen.insert(server_field_type);

    // If |field| is an HTML5 month input, handle it as a special case.
    if (base::EqualsCaseInsensitiveASCII(field->form_control_type, "month")) {
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

IBAN FormDataImporter::ExtractIBANFromForm(const FormStructure& form) {
  IBAN candidate_iban;

  for (const auto& field : form) {
    if (!field->IsFieldFillable() || field->value.empty()) {
      continue;
    }

    AutofillType field_type = field->Type();
    if (field_type.GetStorableType() == IBAN_VALUE &&
        IBAN::IsValid(field->value)) {
      candidate_iban.SetInfo(field_type, field->value, app_locale_);
      break;
    }
  }

  return candidate_iban;
}

absl::optional<std::string> FormDataImporter::ExtractUpiId(
    const FormStructure& form) {
  for (const auto& field : form) {
    if (IsUPIVirtualPaymentAddress(field->value))
      return base::UTF16ToUTF8(field->value);
  }
  return absl::nullopt;
}

bool FormDataImporter::ShouldOfferUploadCardOrLocalCardSave(
    const absl::optional<CreditCard>& credit_card_import_candidate,
    bool is_credit_card_upload_enabled) {
  // If we have an invalid card in the form, a duplicate field type, or we have
  // entered a virtual card, |credit_card_import_candidate| will be set
  // to nullptr and thus we do not want to offer upload save or local card save.
  if (!credit_card_import_candidate)
    return false;

  // We do not want to offer upload save or local card save for server cards.
  if (credit_card_import_type_ == CreditCardImportType::kServerCard) {
    return false;
  }

  // If we have a local card but credit card upload is not enabled, we do not
  // want to offer upload save as it is disabled and we do not want to offer
  // local card save as it is already saved as a local card.
  if (!is_credit_card_upload_enabled &&
      credit_card_import_type_ == CreditCardImportType::kLocalCard) {
    return false;
  }

  // We know |credit_card_import_candidate| is either a new card, or a local
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

}  // namespace autofill
