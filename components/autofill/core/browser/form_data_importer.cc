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
#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_name.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/logging/log_manager.h"
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
bool IsValidFieldTypeAndValue(const std::set<ServerFieldType>& types_seen,
                              ServerFieldType field_type,
                              const base::string16& value,
                              LogBuffer* import_log_buffer) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for PHONE_HOME_NUMBER however as both prefix and
  // suffix are stored against this type, and for EMAIL_ADDRESS because it is
  // common to see second 'confirm email address' fields on forms.
  if (types_seen.count(field_type) && field_type != PHONE_HOME_NUMBER &&
      field_type != EMAIL_ADDRESS) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Multiple fields of type "
                         << AutofillType(field_type).ToString() << "."
                         << CTag{};
    }
    return false;
  }
  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value)) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Email address found in field of different type: "
                         << AutofillType(field_type).ToString() << CTag{};
    }
    return false;
  }

  return true;
}

// Returns true if minimum requirements for import of a given |profile| have
// been met.  An address submitted via a form must have at least the fields
// required as determined by its country code.
// No verification of validity of the contents is performed. This is an
// existence check only.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& variation_country_code,
                      const std::string& app_locale,
                      LogBuffer* import_log_buffer) {
  // Try to acquire the country code form the filled form.
  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));

  if (import_log_buffer && !country_code.empty()) {
    *import_log_buffer << LogMessage::kImportAddressProfileFromFormCountrySource
                       << "Country entry in form." << CTag{};
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseVariationCountryCode)) {
    // As a fallback, use the finch state to get a country code.
    if (country_code.empty() && !variation_country_code.empty()) {
      country_code = variation_country_code;
      if (import_log_buffer && !country_code.empty()) {
        *import_log_buffer
            << LogMessage::kImportAddressProfileFromFormCountrySource
            << "Variations service." << CTag{};
      }
    }
  }

  // As the last resort, derive the country code from the app_locale.
  if (country_code.empty()) {
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);
    if (import_log_buffer && !country_code.empty()) {
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormCountrySource
          << "App locale." << CTag{};
    }
  }

  AutofillCountry country(country_code, app_locale);

  // Include the details of the country to the log.
  if (import_log_buffer)
    *import_log_buffer << country;

  // Check the |ADDRESS_HOME_LINE1| requirement.
  bool is_line1_missing = false;
  if (country.requires_line1() &&
      profile.GetRawInfo(ADDRESS_HOME_LINE1).empty() &&
      profile.GetRawInfo(ADDRESS_HOME_STREET_NAME).empty()) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Missing required ADDRESS_HOME_LINE1." << CTag{};
    }
    is_line1_missing = true;
  }

  // Check the |ADDRESS_HOME_CITY| requirement.
  bool is_city_missing = false;
  if (country.requires_city() &&
      profile.GetRawInfo(ADDRESS_HOME_CITY).empty()) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Missing required ADDRESS_HOME_CITY." << CTag{};
    }
    is_city_missing = true;
  }

  // Check the |ADDRESS_HOME_STATE| requirement.
  bool is_state_missing = false;
  if (country.requires_state() &&
      profile.GetRawInfo(ADDRESS_HOME_STATE).empty()) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Missing required ADDRESS_HOME_STATE." << CTag{};
    }
    is_state_missing = true;
  }

  // Check the |ADDRESS_HOME_ZIP| requirement.
  bool is_zip_missing = false;
  if (country.requires_zip() && profile.GetRawInfo(ADDRESS_HOME_ZIP).empty()) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Missing required ADDRESS_HOME_ZIP." << CTag{};
    }
    is_zip_missing = true;
  }

  bool is_zip_or_state_requirement_violated = false;
  if (country.requires_zip_or_state() &&
      profile.GetRawInfo(ADDRESS_HOME_ZIP).empty() &&
      profile.GetRawInfo(ADDRESS_HOME_STATE).empty()) {
    if (import_log_buffer) {
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormFailed
          << "Missing required ADDRESS_HOME_ZIP or ADDRESS_HOME_STATE."
          << CTag{};
    }
    is_zip_or_state_requirement_violated = true;
  }

  // Collect metrics regarding the requirements.
  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_line1_missing ? AddressImportRequirement::LINE1_REQUIREMENT_VIOLATED
                       : AddressImportRequirement::LINE1_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_city_missing ? AddressImportRequirement::CITY_REQUIREMENT_VIOLATED
                      : AddressImportRequirement::CITY_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_state_missing ? AddressImportRequirement::STATE_REQUIREMENT_VIOLATED
                       : AddressImportRequirement::STATE_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_zip_missing ? AddressImportRequirement::ZIP_REQUIREMENT_VIOLATED
                     : AddressImportRequirement::ZIP_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportRequirementMetric(
      is_zip_or_state_requirement_violated
          ? AddressImportRequirement::ZIP_OR_STATE_REQUIREMENT_VIOLATED
          : AddressImportRequirement::ZIP_OR_STATE_REQUIREMENT_FULFILLED);

  AutofillMetrics::LogAddressFormImportCountrySpecificFieldRequirementsMetric(
      is_zip_missing, is_state_missing, is_city_missing, is_line1_missing);

  // Return true if all requirements are fulfilled.
  return !(is_line1_missing || is_city_missing || is_state_missing ||
           is_zip_missing || is_zip_or_state_requirement_violated);
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
#if !defined(OS_ANDROID) && !defined(OS_IOS)
      local_card_migration_manager_(
          std::make_unique<LocalCardMigrationManager>(client,
                                                      payments_client,
                                                      app_locale,
                                                      personal_data_manager)),
      upi_vpa_save_manager_(
          std::make_unique<UpiVpaSaveManager>(client, personal_data_manager)),
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)
      personal_data_manager_(personal_data_manager),
      app_locale_(app_locale) {
}

FormDataImporter::~FormDataImporter() {}

void FormDataImporter::ImportFormData(const FormStructure& submitted_form,
                                      bool profile_autofill_enabled,
                                      bool credit_card_autofill_enabled) {
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> detected_upi_id;

  bool is_credit_card_upstream_enabled =
      credit_card_save_manager_->IsCreditCardUploadEnabled();
  // ImportFormData will set the |imported_credit_card_record_type_|. If the
  // imported card is invalid or already a server card, or if
  // |credit_card_save_manager_| does not allow uploading,
  // |imported_credit_card| will be nullptr.
  ImportFormData(submitted_form, profile_autofill_enabled,
                 credit_card_autofill_enabled,
                 /*should_return_local_card=*/is_credit_card_upstream_enabled,
                 &imported_credit_card, &detected_upi_id);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (detected_upi_id && credit_card_autofill_enabled &&
      base::FeatureList::IsEnabled(features::kAutofillSaveAndFillVPA)) {
    upi_vpa_save_manager_->OfferLocalSave(*detected_upi_id);
  }
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

  // If no card was successfully imported from the form, return.
  if (imported_credit_card_record_type_ ==
      ImportedCreditCardRecordType::NO_CARD) {
    return;
  }
  // Do not offer upload save for google domain.
  if (net::HasGoogleHost(submitted_form.main_frame_origin().GetURL()) &&
      is_credit_card_upstream_enabled) {
    return;
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // A credit card was successfully imported, but it's possible it is already a
  // local or server card. First, check to see if we should offer local card
  // migration in this case, as local cards could go either way.
  if (local_card_migration_manager_ &&
      local_card_migration_manager_->ShouldOfferLocalCardMigration(
          imported_credit_card.get(), imported_credit_card_record_type_)) {
    local_card_migration_manager_->AttemptToOfferLocalCardMigration(
        /*is_from_settings_page=*/false);
    return;
  }
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

  // Local card migration will not be offered. If we do not have a new card to
  // save (or a local card to upload save), return.
  if (!imported_credit_card)
    return;

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
  } else {
    // If upload save is not allowed, new cards should be saved locally.
    DCHECK(imported_credit_card_record_type_ ==
           ImportedCreditCardRecordType::NEW_CARD);
    credit_card_save_manager_->AttemptToOfferCardLocalSave(
        from_dynamic_change_form_, has_non_focusable_field_,
        *imported_credit_card);
  }
}

CreditCard FormDataImporter::ExtractCreditCardFromForm(
    const FormStructure& form) {
  bool has_duplicate_field_type;
  return ExtractCreditCardFromForm(form, &has_duplicate_field_type);
}

// static
bool FormDataImporter::IsValidLearnableProfile(
    const AutofillProfile& profile,
    const std::string& variation_country_code,
    const std::string& app_locale,
    LogBuffer* import_log_buffer) {
  // Check if the imported address qualifies as a minimum address.
  bool is_not_minimum_address = false;
  if (!IsMinimumAddress(profile, variation_country_code, app_locale,
                        import_log_buffer)) {
    is_not_minimum_address = true;
  }

  // Check that the email address is valid if it is supplied.
  bool is_email_invalid = false;
  base::string16 email = profile.GetRawInfo(EMAIL_ADDRESS);
  if (!email.empty() && !IsValidEmailAddress(email)) {
    if (import_log_buffer) {
      *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                         << "Invalid email address." << CTag{};
    }
    is_email_invalid = true;
  }

  // Reject profiles with an invalid |HOME_ADDRESS_STATE| entry.
  bool is_state_invalid = false;
  if (profile.IsPresentButInvalid(ADDRESS_HOME_STATE)) {
    if (import_log_buffer)
      *import_log_buffer
          << LogMessage::kImportAddressProfileFromFormFailed
          << "Invalid state as of AutofillProfile::IsPresentButInvalid()."
          << CTag{};
    is_state_invalid = true;
  }

  // Reject profiles with an invalid |HOME_ADDRESS_ZIP| entry.
  bool is_zip_invalid = false;
  if (profile.IsPresentButInvalid(ADDRESS_HOME_ZIP)) {
    if (import_log_buffer)
      *import_log_buffer
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
  return !(is_not_minimum_address || is_email_invalid || is_state_invalid ||
           is_zip_invalid);
}

bool FormDataImporter::ImportFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool credit_card_autofill_enabled,
    bool should_return_local_card,
    std::unique_ptr<CreditCard>* imported_credit_card,
    base::Optional<std::string>* imported_upi_id) {
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
  if (profile_autofill_enabled) {
    address_import = ImportAddressProfiles(submitted_form);
  }

  if (cc_import || address_import || imported_upi_id->has_value())
    return true;

  personal_data_manager_->MarkObserversInsufficientFormDataForImport();
  return false;
}

bool FormDataImporter::ImportAddressProfiles(const FormStructure& form) {
  // Create a buffer to collect logging output for the autofill-internals.
  LogBuffer import_log_buffer;
  import_log_buffer << LoggingScope::kAddressProfileFormImport;
  // Print the full form into the logging scope.
  import_log_buffer << LogMessage::kImportAddressProfileFromForm << form
                    << CTag{};

  // We save a maximum of 2 profiles per submitted form (e.g. for shipping and
  // billing).
  static const size_t kMaxNumAddressProfilesSaved = 2;
  size_t num_saved_profiles = 0;

  if (!form.field_count()) {
    import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                      << "Form is empty." << CTag{};
  } else {
    // Relevant sections for address fields.
    std::set<std::string> sections;
    for (const auto& field : form) {
      if (field->Type().group() != CREDIT_CARD)
        sections.insert(field->section);
    }

    for (const std::string& section : sections) {
      if (num_saved_profiles == kMaxNumAddressProfilesSaved)
        break;
      // Log the output from a section in a separate div for readability.
      import_log_buffer << Tag{"div"}
                        << Attrib{"class", "profile_import_from_form_section"};
      import_log_buffer << LogMessage::kImportAddressProfileFromFormSection
                        << section << CTag{};
      // Try to import an address profile from the form fields of this section.
      if (ImportAddressProfileForSection(form, section, &import_log_buffer))
        num_saved_profiles++;
      // And close the div of the section import log.
      import_log_buffer << CTag{"div"};
    }
    // TODO(crbug.com/1097125): Remove feature test.
    // Run the import on the union of the section if the import was not
    // successful and if there is more than one section.
    if (num_saved_profiles > 0) {
      AutofillMetrics::LogAddressFormImportStatustMetric(
          AutofillMetrics::AddressProfileImportStatusMetric::REGULAR_IMPORT);
    } else if (base::FeatureList::IsEnabled(
                   features::kAutofillProfileImportFromUnifiedSection) &&
               sections.size() > 1) {
      // Try to import by combining all sections.
      if (ImportAddressProfileForSection(form, "", &import_log_buffer)) {
        num_saved_profiles++;
        AutofillMetrics::LogAddressFormImportStatustMetric(
            AutofillMetrics::AddressProfileImportStatusMetric::
                SECTION_UNION_IMPORT);
      }
    }
    if (num_saved_profiles == 0) {
      AutofillMetrics::LogAddressFormImportStatustMetric(
          AutofillMetrics::AddressProfileImportStatusMetric::NO_IMPORT);
    }
  }
  import_log_buffer << LogMessage::kImportAddressProfileFromFormNumberOfImports
                    << num_saved_profiles << CTag{};

  // Write log buffer to autofill-internals.
  LogManager* log_manager = client_->GetLogManager();
  if (log_manager)
    log_manager->Log() << std::move(import_log_buffer);

  return num_saved_profiles > 0;
}

bool FormDataImporter::ImportAddressProfileForSection(
    const FormStructure& form,
    const std::string& section,
    LogBuffer* import_log_buffer) {
  // The candidate for profile import. There are many ways for the candidate to
  // be rejected (see everywhere this function returns false).
  AutofillProfile candidate_profile;

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper combined_phone;

  // Used to detect and discard address forms with multiple fields of the same
  // type.
  std::set<ServerFieldType> types_seen;

  // Tracks if the form section contains multiple distinct email addresses.
  bool has_multiple_distinct_email_addresses = false;

  // Tracks if the form section contains an invalid types.
  bool has_invalid_field_types = false;

  // Tracks if the form section contains an invalid phone number.
  bool has_invalid_phone_number = false;

  // Tracks if the form section contains an invalid country.
  bool has_invalid_country = false;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const auto& field : form) {
    // Reject fields that are not within the specified |section|.
    // If section is empty, use all fields.
    if (field->section != section && !section.empty())
      continue;

    base::string16 value;
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
    if (field_type.group() == CREDIT_CARD)
      continue;

    // There can be multiple email fields (e.g. in the case of 'confirm email'
    // fields) but they must all contain the same value, else the profile is
    // invalid.
    ServerFieldType server_field_type = field_type.GetStorableType();
    if (server_field_type == EMAIL_ADDRESS &&
        types_seen.count(server_field_type) &&
        candidate_profile.GetRawInfo(EMAIL_ADDRESS) != value) {
      if (import_log_buffer) {
        *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                           << "Multiple different email addresses present."
                           << CTag{};
      }
      has_multiple_distinct_email_addresses = true;
    }

    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(types_seen, server_field_type, value,
                                  import_log_buffer))
      has_invalid_field_types = true;
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
        candidate_profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty()) {
      // TODO(crbug.com/1075604): Remove branch with disabled feature.
      if (base::FeatureList::IsEnabled(
              features::kAutofillUsePageLanguageToTranslateCountryNames)) {
        // The country code was not successfully determined from the value in
        // the country field. This can be caused by a localization that does not
        // match the |app_locale|. Try setting the value again using the
        // language of the page. Note, there should be a locale associated with
        // every language code.
        std::string page_language;
        const translate::LanguageState* language_state =
            client_->GetLanguageState();
        if (language_state)
          page_language = language_state->original_language();
        // Retry to set the country of there is known page language.
        if (!page_language.empty()) {
          candidate_profile.SetInfoWithVerificationStatus(
              field_type, value, page_language, VerificationStatus::kObserved);
        }
      }
      // Check if the country code was still not determined correctly.
      if (candidate_profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty()) {
        if (import_log_buffer) {
          *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                             << "Missing country." << CTag{};
        }
        has_invalid_country = true;
      }
    }
  }

  // Construct the phone number. Reject the whole profile if the number is
  // invalid.
  if (!combined_phone.IsEmpty()) {
    base::string16 constructed_number;
    if (!combined_phone.ParseNumber(candidate_profile, app_locale_,
                                    &constructed_number) ||
        !candidate_profile.SetInfoWithVerificationStatus(
            AutofillType(PHONE_HOME_WHOLE_NUMBER), constructed_number,
            app_locale_, VerificationStatus::kObserved)) {
      if (import_log_buffer) {
        *import_log_buffer << LogMessage::kImportAddressProfileFromFormFailed
                           << "Invalid phone number." << CTag{};
      }
      has_invalid_phone_number = true;
    }
  }

  // Reject the profile if minimum address and validation requirements are not
  // met.
  const std::string variation_country_code =
      client_->GetVariationConfigCountryCode();
  bool is_invalid_learnable_profile =
      !IsValidLearnableProfile(candidate_profile, variation_country_code,
                               app_locale_, import_log_buffer);

  // Do not import a profile if any of the requirements is violated.
  bool all_fullfilled =
      !(has_multiple_distinct_email_addresses || has_invalid_field_types ||
        has_invalid_country || has_invalid_phone_number ||
        is_invalid_learnable_profile);

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
      all_fullfilled ? AddressImportRequirement::OVERALL_REQUIREMENT_FULFILLED
                     : AddressImportRequirement::OVERALL_REQUIREMENT_VIOLATED);

  if (!all_fullfilled)
    return false;

  if (!candidate_profile.FinalizeAfterImport())
    return false;

  std::string guid =
      personal_data_manager_->SaveImportedProfile(candidate_profile);

  return !guid.empty();
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

  // Can import one valid card per form. Start by treating it as NEW_CARD, but
  // overwrite this type if we discover it is already a local or server card.
  imported_credit_card_record_type_ = ImportedCreditCardRecordType::NEW_CARD;

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
      personal_data_manager_->UpdateCreditCard(card_copy);
      // Mark that the credit card imported from the submitted form is
      // already a local card.
      imported_credit_card_record_type_ =
          ImportedCreditCardRecordType::LOCAL_CARD;

      // If the card is a local card and it has a nickname stored in the local
      // database, copy the nickname to the |candidate_credit_card| so that the
      // nickname also shows in the Upstream bubble.
      candidate_credit_card.SetNickname(card_copy.nickname());

      // If we should not return the local card, return that we merged it,
      // without setting |imported_credit_card|.
      if (!should_return_local_card)
        return true;

      break;
    }
  }

  // Also don't offer to save if we already have this stored as a server
  // card. We only check the number because if the new card has the same number
  // as the server card, upload is guaranteed to fail. There's no mechanism for
  // entries with the same number but different names or expiration dates as
  // there is for local cards.
  for (const CreditCard* card :
       personal_data_manager_->GetServerCreditCards()) {
    if ((card->record_type() == CreditCard::MASKED_SERVER_CARD &&
         card->LastFourDigits() == candidate_credit_card.LastFourDigits()) ||
        (card->record_type() == CreditCard::FULL_SERVER_CARD &&
         candidate_credit_card.HasSameNumberAs(*card))) {
      // Don't update card if the expiration date is missing
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
      return false;
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

  std::set<ServerFieldType> types_seen;
  for (const auto& field : form) {
    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, then skip it.
    if (!field->IsFieldFillable() || value.empty())
      continue;

    AutofillType field_type = field->Type();
    // Field was not identified as a credit card field.
    if (field_type.group() != CREDIT_CARD)
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
    if (base::LowerCaseEqualsASCII(field->form_control_type, "month")) {
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
      for (size_t i = 0; i < field->option_contents.size(); ++i) {
        if (value == field->option_contents[i]) {
          candidate_credit_card.SetInfo(field_type, field->option_values[i],
                                        app_locale_);
          break;
        }
      }
    }
  }

  return candidate_credit_card;
}

base::Optional<std::string> FormDataImporter::ImportUpiId(
    const FormStructure& form) {
  for (const auto& field : form) {
    if (IsUPIVirtualPaymentAddress(field->value))
      return base::UTF16ToUTF8(field->value);
  }
  return base::nullopt;
}

}  // namespace autofill
