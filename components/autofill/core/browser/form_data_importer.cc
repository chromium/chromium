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
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/phone_number.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"

namespace autofill {

namespace {

// Return true if the |field_type| and |value| are valid within the context
// of importing a form.
bool IsValidFieldTypeAndValue(const std::set<ServerFieldType>& types_seen,
                              ServerFieldType field_type,
                              const base::string16& value) {
  // Abandon the import if two fields of the same type are encountered.
  // This indicates ambiguous data or miscategorization of types.
  // Make an exception for PHONE_HOME_NUMBER however as both prefix and
  // suffix are stored against this type, and for EMAIL_ADDRESS because it is
  // common to see second 'confirm email address' fields on forms.
  if (types_seen.count(field_type) && field_type != PHONE_HOME_NUMBER &&
      field_type != EMAIL_ADDRESS)
    return false;

  // Abandon the import if an email address value shows up in a field that is
  // not an email address.
  if (field_type != EMAIL_ADDRESS && IsValidEmailAddress(value))
    return false;

  return true;
}

// Returns true if minimum requirements for import of a given |profile| have
// been met.  An address submitted via a form must have at least the fields
// required as determined by its country code.
// No verification of validity of the contents is performed. This is an
// existence check only.
bool IsMinimumAddress(const AutofillProfile& profile,
                      const std::string& app_locale) {
  // All countries require at least one address line.
  if (profile.GetRawInfo(ADDRESS_HOME_LINE1).empty())
    return false;

  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  if (country_code.empty())
    country_code = AutofillCountry::CountryCodeForLocale(app_locale);

  AutofillCountry country(country_code, app_locale);

  if (country.requires_city() && profile.GetRawInfo(ADDRESS_HOME_CITY).empty())
    return false;

  if (country.requires_state() &&
      profile.GetRawInfo(ADDRESS_HOME_STATE).empty())
    return false;

  if (country.requires_zip() && profile.GetRawInfo(ADDRESS_HOME_ZIP).empty())
    return false;

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
      upi_vpa_save_manager_(
          std::make_unique<UpiVpaSaveManager>(personal_data_manager)),
      local_card_migration_manager_(
          std::make_unique<LocalCardMigrationManager>(client,
                                                      payments_client,
                                                      app_locale,
                                                      personal_data_manager)),

      personal_data_manager_(personal_data_manager),
      app_locale_(app_locale) {}

FormDataImporter::~FormDataImporter() {}

void FormDataImporter::ImportFormData(const FormStructure& submitted_form,
                                      bool profile_autofill_enabled,
                                      bool credit_card_autofill_enabled) {
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> detected_vpa;

  bool is_credit_card_upstream_enabled =
      credit_card_save_manager_->IsCreditCardUploadEnabled();
  // ImportFormData will set the |imported_credit_card_record_type_|. If the
  // imported card is invalid or already a server card, or if
  // |credit_card_save_manager_| does not allow uploading,
  // |imported_credit_card| will be nullptr.
  ImportFormData(submitted_form, profile_autofill_enabled,
                 credit_card_autofill_enabled,
                 /*should_return_local_card=*/is_credit_card_upstream_enabled,
                 &imported_credit_card, &detected_vpa);

  if (detected_vpa && credit_card_autofill_enabled &&
      base::FeatureList::IsEnabled(features::kAutofillSaveAndFillVPA)) {
    upi_vpa_save_manager_->OfferLocalSave(*detected_vpa);
  }

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
bool FormDataImporter::IsValidLearnableProfile(const AutofillProfile& profile,
                                               const std::string& app_locale) {
  if (!IsMinimumAddress(profile, app_locale))
    return false;

  base::string16 email = profile.GetRawInfo(EMAIL_ADDRESS);
  if (!email.empty() && !IsValidEmailAddress(email))
    return false;

  // Reject profiles with invalid US state information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_STATE))
    return false;

  // Reject profiles with invalid US zip information.
  if (profile.IsPresentButInvalid(ADDRESS_HOME_ZIP))
    return false;

  return true;
}

bool FormDataImporter::ImportFormData(
    const FormStructure& submitted_form,
    bool profile_autofill_enabled,
    bool credit_card_autofill_enabled,
    bool should_return_local_card,
    std::unique_ptr<CreditCard>* imported_credit_card,
    base::Optional<std::string>* imported_vpa) {
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
    *imported_vpa = ImportVPA(submitted_form);
  }
  // - ImportAddressProfiles may eventually save or update one or more address
  //   profiles.
  bool address_import = false;
  if (profile_autofill_enabled) {
    address_import = ImportAddressProfiles(submitted_form);
  }

  if (cc_import || address_import || imported_vpa->has_value())
    return true;

  personal_data_manager_->MarkObserversInsufficientFormDataForImport();
  return false;
}

bool FormDataImporter::ImportAddressProfiles(const FormStructure& form) {
  if (!form.field_count())
    return false;

  // Relevant sections for address fields.
  std::set<std::string> sections;
  for (const auto& field : form) {
    if (field->Type().group() != CREDIT_CARD)
      sections.insert(field->section);
  }

  // We save a maximum of 2 profiles per submitted form (e.g. for shipping and
  // billing).
  static const size_t kMaxNumAddressProfilesSaved = 2;
  size_t num_saved_profiles = 0;
  for (const std::string& section : sections) {
    if (num_saved_profiles == kMaxNumAddressProfilesSaved)
      break;

    if (ImportAddressProfileForSection(form, section))
      num_saved_profiles++;
  }

  return num_saved_profiles > 0;
}

bool FormDataImporter::ImportAddressProfileForSection(
    const FormStructure& form,
    const std::string& section) {
  // The candidate for profile import. There are many ways for the candidate to
  // be rejected (see everywhere this function returns false).
  AutofillProfile candidate_profile;

  // We only set complete phone, so aggregate phone parts in these vars and set
  // complete at the end.
  PhoneNumber::PhoneCombineHelper combined_phone;

  // Used to detect and discard address forms with multiple fields of the same
  // type.
  std::set<ServerFieldType> types_seen;

  // Go through each |form| field and attempt to constitute a valid profile.
  for (const auto& field : form) {
    // Reject fields that are not within the specified |section|.
    if (field->section != section)
      continue;

    base::string16 value;
    base::TrimWhitespace(field->value, base::TRIM_ALL, &value);

    // If we don't know the type of the field, or the user hasn't entered any
    // information into the field, or the field is non-focusable (hidden), then
    // skip it.
    if (!field->IsFieldFillable() || !field->is_focusable || value.empty())
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
        candidate_profile.GetRawInfo(EMAIL_ADDRESS) != value)
      return false;

    // If the field type and |value| don't pass basic validity checks then
    // abandon the import.
    if (!IsValidFieldTypeAndValue(types_seen, server_field_type, value))
      return false;
    types_seen.insert(server_field_type);

    // We need to store phone data in the variables, before building the whole
    // number at the end. If |value| is not from a phone field, home.SetInfo()
    // returns false and data is stored directly in |candidate_profile|.
    if (!combined_phone.SetInfo(field_type, value))
      candidate_profile.SetInfo(field_type, value, app_locale_);

    // Reject profiles with invalid country information.
    if (server_field_type == ADDRESS_HOME_COUNTRY &&
        candidate_profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty())
      return false;
  }

  // Construct the phone number. Reject the whole profile if the number is
  // invalid.
  if (!combined_phone.IsEmpty()) {
    base::string16 constructed_number;
    if (!combined_phone.ParseNumber(candidate_profile, app_locale_,
                                    &constructed_number) ||
        !candidate_profile.SetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER),
                                   constructed_number, app_locale_)) {
      return false;
    }
  }

  // Reject the profile if minimum address and validation requirements are not
  // met.
  if (!IsValidLearnableProfile(candidate_profile, app_locale_))
    return false;

  // Delaying |SaveImportedProfile| is safe here because PersonalDataManager
  // outlives this class.
  client_->ConfirmSaveAutofillProfile(
      candidate_profile,
      base::BindOnce(
          base::IgnoreResult(&PersonalDataManager::SaveImportedProfile),
          base::Unretained(personal_data_manager_), candidate_profile));

  return true;
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

  // If editable expiration date experiment is enabled, the card with invalid
  // expiration date can be uploaded. However, the card with invalid card number
  // must be ignored.
  if (!candidate_credit_card.HasValidCardNumber()) {
    return false;
  }
  if (!candidate_credit_card.HasValidExpirationDate() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillUpstreamEditableExpirationDate)) {
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

base::Optional<std::string> FormDataImporter::ImportVPA(
    const FormStructure& form) {
  for (const auto& field : form) {
    if (IsUPIVirtualPaymentAddress(field->value))
      return base::UTF16ToUTF8(field->value);
  }
  return base::nullopt;
}

}  // namespace autofill
