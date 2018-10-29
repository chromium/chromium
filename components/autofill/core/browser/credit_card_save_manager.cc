// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_save_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "url/gurl.h"

namespace autofill {

namespace {

// If |name| consists of three whitespace-separated parts and the second of the
// three parts is a single character or a single character followed by a period,
// returns the result of joining the first and third parts with a space.
// Otherwise, returns |name|.
//
// Note that a better way to do this would be to use SplitName from
// src/components/autofill/core/browser/contact_info.cc. However, for now we
// want the logic of which variations of names are considered to be the same to
// exactly match the logic applied on the Payments server.
base::string16 RemoveMiddleInitial(const base::string16& name) {
  std::vector<base::StringPiece16> parts =
      base::SplitStringPiece(name, base::kWhitespaceUTF16,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 3 && (parts[1].length() == 1 ||
                            (parts[1].length() == 2 &&
                             base::EndsWith(parts[1], base::ASCIIToUTF16("."),
                                            base::CompareCase::SENSITIVE)))) {
    parts.erase(parts.begin() + 1);
    return base::JoinString(parts, base::ASCIIToUTF16(" "));
  }
  return name;
}

}  // namespace

CreditCardSaveManager::CreditCardSaveManager(
    AutofillClient* client,
    payments::PaymentsClient* payments_client,
    const std::string& app_locale,
    PersonalDataManager* personal_data_manager)
    : client_(client),
      payments_client_(payments_client),
      app_locale_(app_locale),
      personal_data_manager_(personal_data_manager),
      weak_ptr_factory_(this) {}

CreditCardSaveManager::~CreditCardSaveManager() {}

void CreditCardSaveManager::AttemptToOfferCardLocalSave(
    const CreditCard& card) {
  local_card_save_candidate_ = card;
  show_save_prompt_ = base::nullopt;

  // Query the Autofill StrikeDatabase on if we should pop up the offer-to-save
  // prompt for this card.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCreditCardUsesStrikeSystem)) {
    StrikeDatabase* strike_database = client_->GetStrikeDatabase();
    strike_database->GetStrikes(
        strike_database->GetKeyForCreditCardSave(
            base::UTF16ToUTF8(local_card_save_candidate_.LastFourDigits())),
        base::BindRepeating(&CreditCardSaveManager::OnDidGetStrikesForLocalSave,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Skip retrieving data from the Autofill StrikeDatabase; assume 0 strikes.
    OnDidGetStrikesForLocalSave(0);
  }
}

void CreditCardSaveManager::AttemptToOfferCardUploadSave(
    const FormStructure& submitted_form,
    const CreditCard& card,
    const bool uploading_local_card) {
  // Abort the uploading if |payments_client_| is nullptr.
  if (!payments_client_)
    return;
  upload_request_ = payments::PaymentsClient::UploadRequestDetails();
  upload_request_.card = card;
  uploading_local_card_ = uploading_local_card;
  show_save_prompt_ = base::nullopt;

  // In an ideal scenario, when uploading a card, we would have:
  //  1) Card number and expiration
  //  2) CVC
  //  3) 1+ recently-used or modified addresses that meet validation rules (or
  //     only the address countries if the relevant feature is enabled).
  //  4) Cardholder name or names on the address profiles
  // At a minimum, only #1 (card number and expiration) is absolutely required
  // in order to save a card to Google Payments. We perform all checks before
  // returning or logging in order to know where we stand with regards to card
  // upload information. Then, we ping Google Payments and ask if upload save
  // should be offered with the given amount of information, letting Payments
  // make the final offer-to-save decision.
  found_cvc_field_ = false;
  found_value_in_cvc_field_ = false;
  found_cvc_value_in_non_cvc_field_ = false;

  for (const auto& field : submitted_form) {
    const bool is_valid_cvc = IsValidCreditCardSecurityCode(
        field->value, upload_request_.card.network());
    if (field->Type().GetStorableType() == CREDIT_CARD_VERIFICATION_CODE) {
      found_cvc_field_ = true;
      if (!field->value.empty())
        found_value_in_cvc_field_ = true;
      if (is_valid_cvc) {
        upload_request_.cvc = field->value;
        break;
      }
    } else if (is_valid_cvc &&
               field->Type().GetStorableType() == UNKNOWN_TYPE) {
      found_cvc_value_in_non_cvc_field_ = true;
    }
  }

  // Upload requires that recently used or modified addresses meet the
  // client-side validation rules. This call also begins setting the value of
  // |upload_decision_metrics_|.
  SetProfilesForCreditCardUpload(card, &upload_request_);

  pending_upload_request_origin_ = submitted_form.main_frame_origin();

  if (upload_request_.cvc.empty()) {
    // Apply the CVC decision to |upload_decision_metrics_| to denote a problem
    // was found.
    upload_decision_metrics_ |= GetCVCCardUploadDecisionMetric();
  }

  // Add active Chrome experiments to the request payload here (currently none).

  // We store the detected values in the upload request, because the addresses
  // are being possibly modified in the next code block, and we want the
  // detected values to reflect addresses *before* they are modified.
  upload_request_.detected_values = GetDetectedValues();
  // If the user must provide cardholder name, log it and set
  // |should_request_name_from_user_| so the offer-to-save dialog know to ask
  // for it.
  should_request_name_from_user_ = false;
  if (upload_request_.detected_values & DetectedValue::USER_PROVIDED_NAME) {
    upload_decision_metrics_ |=
        AutofillMetrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME;
    should_request_name_from_user_ = true;
  }

  // If the relevant feature is enabled, only send the country of the
  // recently-used addresses. We make a copy here to avoid modifying
  // |upload_request_.profiles|, which should always have full addresses even
  // after this function goes out of scope.
  bool send_only_country_in_addresses = base::FeatureList::IsEnabled(
      features::kAutofillSendOnlyCountryInGetUploadDetails);
  std::vector<AutofillProfile> country_only_profiles;
  if (send_only_country_in_addresses) {
    for (const AutofillProfile& address : upload_request_.profiles) {
      AutofillProfile country_only;
      country_only.SetInfo(ADDRESS_HOME_COUNTRY,
                           address.GetInfo(ADDRESS_HOME_COUNTRY, app_locale_),
                           app_locale_);
      country_only_profiles.emplace_back(std::move(country_only));
    }
  }

  // All required data is available, start the upload process.
  if (observer_for_testing_)
    observer_for_testing_->OnDecideToRequestUploadSave();
  payments_client_->GetUploadDetails(
      send_only_country_in_addresses ? country_only_profiles
                                     : upload_request_.profiles,
      upload_request_.detected_values, upload_request_.active_experiments,
      app_locale_,
      base::BindOnce(&CreditCardSaveManager::OnDidGetUploadDetails,
                     weak_ptr_factory_.GetWeakPtr()),
      payments::kUploadCardBillableServiceNumber);
  // Query the Autofill StrikeDatabase on if we should pop up the offer-to-save
  // prompt for this card.
  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCreditCardUsesStrikeSystem)) {
    StrikeDatabase* strike_database = client_->GetStrikeDatabase();
    strike_database->GetStrikes(
        strike_database->GetKeyForCreditCardSave(
            base::UTF16ToUTF8(upload_request_.card.LastFourDigits())),
        base::BindRepeating(
            &CreditCardSaveManager::OnDidGetStrikesForUploadSave,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Skip retrieving data from the Autofill StrikeDatabase; assume 0 strikes.
    OnDidGetStrikesForUploadSave(0);
  }
}

bool CreditCardSaveManager::IsCreditCardUploadEnabled() {
  // If observer_for_testing_ is set, assume we are in a browsertest and
  // credit card upload should be enabled by default.
  return observer_for_testing_ ||
         ::autofill::IsCreditCardUploadEnabled(
             client_->GetPrefs(), client_->GetSyncService(),
             personal_data_manager_->GetAccountInfoForPaymentsServer().email);
}

bool CreditCardSaveManager::IsUploadEnabledForNetwork(
    const std::string& network) {
  if (network == kEloCard &&
      base::FeatureList::IsEnabled(features::kAutofillUpstreamDisallowElo)) {
    return false;
  } else if (network == kJCBCard &&
             base::FeatureList::IsEnabled(
                 features::kAutofillUpstreamDisallowJcb)) {
    return false;
  }
  return true;
}

void CreditCardSaveManager::OnDidUploadCard(
    AutofillClient::PaymentsRpcResult result,
    const std::string& server_id) {
  if (observer_for_testing_)
    observer_for_testing_->OnReceivedUploadCardResponse();

  if (result == AutofillClient::SUCCESS &&
      upload_request_.card.HasFirstAndLastName()) {
    AutofillMetrics::LogSaveCardWithFirstAndLastNameComplete(
        /*is_local=*/false);
  }

  if (result == AutofillClient::SUCCESS) {
    // If the upload succeeds and we can store unmasked cards on this OS, we
    // will keep a copy of the card as a full server card on the device.
    if (!server_id.empty() && OfferStoreUnmaskedCards() &&
        !IsAutofillNoLocalSaveOnUploadSuccessExperimentEnabled()) {
      upload_request_.card.set_record_type(CreditCard::FULL_SERVER_CARD);
      upload_request_.card.SetServerStatus(CreditCard::OK);
      upload_request_.card.set_server_id(server_id);
      DCHECK(personal_data_manager_);
      if (personal_data_manager_)
        personal_data_manager_->AddFullServerCreditCard(upload_request_.card);
    }
    if (base::FeatureList::IsEnabled(
            features::kAutofillSaveCreditCardUsesStrikeSystem)) {
      StrikeDatabase* strike_database = client_->GetStrikeDatabase();
      // Log how many strikes the card had when it was saved.
      strike_database->GetStrikes(
          strike_database->GetKeyForCreditCardSave(
              base::UTF16ToUTF8(upload_request_.card.LastFourDigits())),
          base::BindRepeating(
              &CreditCardSaveManager::LogStrikesPresentWhenCardSaved,
              weak_ptr_factory_.GetWeakPtr(),
              /*is_local=*/false));
      // Clear all strikes for this card, in case it is later removed.
      strike_database->ClearAllStrikesForKey(
          strike_database->GetKeyForCreditCardSave(
              base::UTF16ToUTF8(upload_request_.card.LastFourDigits())),
          base::DoNothing());
    }
  } else {
    if (base::FeatureList::IsEnabled(
            features::kAutofillSaveCreditCardUsesStrikeSystem) &&
        show_save_prompt_.value()) {
      // If the upload failed and the bubble was actually shown (NOT just the
      // icon), count that as a strike against offering upload in the future.
      StrikeDatabase* strike_database = client_->GetStrikeDatabase();
      strike_database->AddStrike(
          strike_database->GetKeyForCreditCardSave(
              base::UTF16ToUTF8(upload_request_.card.LastFourDigits())),
          base::BindRepeating(&CreditCardSaveManager::OnStrikeChangeComplete,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void CreditCardSaveManager::OnDidGetStrikesForLocalSave(const int num_strikes) {
  show_save_prompt_ =
      num_strikes < kMaxStrikesToPreventPoppingUpOfferToSavePrompt;

  OfferCardLocalSave();
}

void CreditCardSaveManager::OnDidGetStrikesForUploadSave(
    const int num_strikes) {
  show_save_prompt_ =
      num_strikes < kMaxStrikesToPreventPoppingUpOfferToSavePrompt;

  // Only offer upload once both Payments and the Autofill StrikeDatabase have
  // returned their decisions. Use population of |upload_request_.context_token|
  // as an indicator of the Payments call returning successfully.
  if (!upload_request_.context_token.empty())
    OfferCardUploadSave();
}

void CreditCardSaveManager::OnDidGetUploadDetails(
    AutofillClient::PaymentsRpcResult result,
    const base::string16& context_token,
    std::unique_ptr<base::DictionaryValue> legal_message) {
  if (observer_for_testing_)
    observer_for_testing_->OnReceivedGetUploadDetailsResponse();
  if (result == AutofillClient::SUCCESS) {
    // Do *not* call payments_client_->Prepare() here. We shouldn't send
    // credentials until the user has explicitly accepted a prompt to upload.
    upload_request_.context_token = context_token;
    legal_message_ = std::move(legal_message);

    // Only offer upload once both Payments and the Autofill StrikeDatabase have
    // returned their decisions. Use presence of |show_save_prompt_| as an
    // indicator of StrikeDatabase retrieving its data.
    if (show_save_prompt_ != base::nullopt)
      OfferCardUploadSave();
  } else {
    // If the upload details request failed and we *know* we have all possible
    // information (card number, expiration, cvc, name, and address), fall back
    // to a local save (for new cards only). It indicates that "Payments doesn't
    // want this card" or "Payments doesn't currently support this country", in
    // which case the upload details request will consistently fail and if we
    // don't fall back to a local save, the user will never be offered *any*
    // kind of credit card save. (Note that this could intermittently backfire
    // if there's a network breakdown or Payments outage, resulting in sometimes
    // showing upload and sometimes offering local save, but such cases should
    // be rare.)
    //
    // Note that calling AttemptToOfferCardLocalSave(~) pings the Autofill
    // StrikeDatabase again, but A) the result should be cached so this
    // shouldn't hit the disk, and B) the alternative would require hooking into
    // the StrikeDatabase's GetStrikes() call already in progress, which would
    // be hacky at worst and require additional class state variables at best.
    bool found_name_and_postal_code_and_cvc =
        (upload_request_.detected_values & DetectedValue::CARDHOLDER_NAME ||
         upload_request_.detected_values & DetectedValue::ADDRESS_NAME) &&
        upload_request_.detected_values & DetectedValue::POSTAL_CODE &&
        upload_request_.detected_values & DetectedValue::CVC;
    if (found_name_and_postal_code_and_cvc && !uploading_local_card_)
      AttemptToOfferCardLocalSave(upload_request_.card);
    upload_decision_metrics_ |=
        AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED;
    LogCardUploadDecisions(upload_decision_metrics_);
  }
}

void CreditCardSaveManager::OfferCardLocalSave() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  bool is_mobile_build = true;
#else
  bool is_mobile_build = false;
#endif  // #if defined(OS_ANDROID) || defined(OS_IOS)

  // If |show_save_prompt_.value()| is false, desktop builds will still offer
  // save in the omnibox without popping-up the bubble. Mobile builds, however,
  // should not display the offer-to-save infobar at all.
  if (!is_mobile_build || show_save_prompt_.value()) {
    if (observer_for_testing_)
      observer_for_testing_->OnOfferLocalSave();
    client_->ConfirmSaveCreditCardLocally(
        local_card_save_candidate_, show_save_prompt_.value(),
        base::BindOnce(&CreditCardSaveManager::OnUserDidAcceptLocalSave,
                       weak_ptr_factory_.GetWeakPtr()));

    // Log metrics.
    if (local_card_save_candidate_.HasFirstAndLastName())
      AutofillMetrics::LogSaveCardWithFirstAndLastNameOffered(
          /*is_local=*/true);
  }
  if (!show_save_prompt_.value()) {
    AutofillMetrics::LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
        AutofillMetrics::SaveTypeMetric::LOCAL);
  }
}

void CreditCardSaveManager::OfferCardUploadSave() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  bool is_mobile_build = true;
#else
  bool is_mobile_build = false;
#endif  // #if defined(OS_ANDROID) || defined(OS_IOS)

  // If |show_save_prompt_.value()| is false, desktop builds will still offer
  // save in the omnibox without popping-up the bubble. Mobile builds, however,
  // should not display the offer-to-save infobar at all.
  if (!is_mobile_build || show_save_prompt_.value()) {
    user_did_accept_upload_prompt_ = false;
    client_->ConfirmSaveCreditCardToCloud(
        upload_request_.card, std::move(legal_message_),
        should_request_name_from_user_, show_save_prompt_.value(),
        base::BindOnce(&CreditCardSaveManager::OnUserDidAcceptUpload,
                       weak_ptr_factory_.GetWeakPtr()));
    client_->LoadRiskData(
        base::BindOnce(&CreditCardSaveManager::OnDidGetUploadRiskData,
                       weak_ptr_factory_.GetWeakPtr()));

    // Log metrics.
    AutofillMetrics::LogUploadOfferedCardOriginMetric(
        uploading_local_card_ ? AutofillMetrics::OFFERING_UPLOAD_OF_LOCAL_CARD
                              : AutofillMetrics::OFFERING_UPLOAD_OF_NEW_CARD);
    if (upload_request_.card.HasFirstAndLastName()) {
      AutofillMetrics::LogSaveCardWithFirstAndLastNameOffered(
          /*is_local=*/false);
    }
    // Set that upload was offered.
    upload_decision_metrics_ |= AutofillMetrics::UPLOAD_OFFERED;
  } else {
    // Set that upload was abandoned due to the Autofill StrikeDatabase
    // returning too many strikes for a mobile infobar to be displayed.
    upload_decision_metrics_ |=
        AutofillMetrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE;
  }
  LogCardUploadDecisions(upload_decision_metrics_);
  if (!show_save_prompt_.value()) {
    AutofillMetrics::LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
        AutofillMetrics::SaveTypeMetric::SERVER);
  }
}

void CreditCardSaveManager::OnUserDidAcceptLocalSave() {
  if (local_card_save_candidate_.HasFirstAndLastName())
    AutofillMetrics::LogSaveCardWithFirstAndLastNameComplete(/*is_local=*/true);

  if (base::FeatureList::IsEnabled(
          features::kAutofillSaveCreditCardUsesStrikeSystem)) {
    StrikeDatabase* strike_database = client_->GetStrikeDatabase();
    // Log how many strikes the card had when it was saved.
    strike_database->GetStrikes(
        strike_database->GetKeyForCreditCardSave(
            base::UTF16ToUTF8(local_card_save_candidate_.LastFourDigits())),
        base::BindRepeating(
            &CreditCardSaveManager::LogStrikesPresentWhenCardSaved,
            weak_ptr_factory_.GetWeakPtr(),
            /*is_local=*/true));
    // Clear all strikes for this card, in case it is later removed.
    strike_database->ClearAllStrikesForKey(
        strike_database->GetKeyForCreditCardSave(
            base::UTF16ToUTF8(local_card_save_candidate_.LastFourDigits())),
        base::DoNothing());
  }

  personal_data_manager_->OnAcceptedLocalCreditCardSave(
      local_card_save_candidate_);
}

void CreditCardSaveManager::LogStrikesPresentWhenCardSaved(
    bool is_local,
    const int num_strikes) {
  std::string suffix = is_local ? "StrikesPresentWhenLocalCardSaved"
                                : "StrikesPresentWhenServerCardSaved";
  base::UmaHistogramCounts1000("Autofill.StrikeDatabase." + suffix,
                               num_strikes);
}

void CreditCardSaveManager::SetProfilesForCreditCardUpload(
    const CreditCard& card,
    payments::PaymentsClient::UploadRequestDetails* upload_request) {
  std::vector<AutofillProfile> candidate_profiles;
  const base::Time now = AutofillClock::Now();
  const base::TimeDelta fifteen_minutes = base::TimeDelta::FromMinutes(15);
  // Reset |upload_decision_metrics_| to begin logging detected problems.
  upload_decision_metrics_ = 0;
  bool has_profile = false;
  bool has_modified_profile = false;

  // First, collect all of the addresses used or modified recently.
  for (AutofillProfile* profile : personal_data_manager_->GetProfiles()) {
    has_profile = true;
    if ((now - profile->modification_date()) < fifteen_minutes) {
      has_modified_profile = true;
      candidate_profiles.push_back(*profile);
    } else if ((now - profile->use_date()) < fifteen_minutes) {
      candidate_profiles.push_back(*profile);
    }
  }

  AutofillMetrics::LogHasModifiedProfileOnCreditCardFormSubmission(
      has_modified_profile);

  if (candidate_profiles.empty()) {
    upload_decision_metrics_ |=
        has_profile
            ? AutofillMetrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS
            : AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE;
  }

  // If any of the names on the card or the addresses don't match the
  // candidate set is invalid. This matches the rules for name matching applied
  // server-side by Google Payments and ensures that we don't send upload
  // requests that are guaranteed to fail.
  const base::string16 card_name =
      card.GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), app_locale_);
  base::string16 verified_name;
  if (candidate_profiles.empty()) {
    verified_name = card_name;
  } else {
    bool found_conflicting_names = false;
    verified_name = RemoveMiddleInitial(card_name);
    for (const AutofillProfile& profile : candidate_profiles) {
      const base::string16 address_name =
          RemoveMiddleInitial(profile.GetInfo(NAME_FULL, app_locale_));
      if (address_name.empty())
        continue;
      if (verified_name.empty()) {
        verified_name = address_name;
      } else if (!base::EqualsCaseInsensitiveASCII(verified_name,
                                                   address_name)) {
        found_conflicting_names = true;
        break;
      }
    }
    if (found_conflicting_names) {
      upload_decision_metrics_ |=
          AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES;
    }
  }

  // If neither the card nor any of the addresses have a name associated with
  // them, the candidate set is invalid.
  if (verified_name.empty()) {
    upload_decision_metrics_ |= AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME;
  }

  // If any of the candidate addresses have a non-empty zip that doesn't match
  // any other non-empty zip, then the candidate set is invalid.
  base::string16 verified_zip;
  const AutofillType kZipCode(ADDRESS_HOME_ZIP);
  for (const AutofillProfile& profile : candidate_profiles) {
    const base::string16 zip = profile.GetRawInfo(ADDRESS_HOME_ZIP);
    if (!zip.empty()) {
      if (verified_zip.empty()) {
        verified_zip = zip;
      } else {
        // To compare two zips, we check to see if either is a prefix of the
        // other. This allows us to consider a 5-digit zip and a zip+4 to be a
        // match if the first 5 digits are the same without hardcoding any
        // specifics of how postal codes are represented. (They can be numeric
        // or alphanumeric and vary from 3 to 10 digits long by country. See
        // https://en.wikipedia.org/wiki/Postal_code#Presentation.) The Payments
        // backend will apply a more sophisticated address-matching procedure.
        // This check is simply meant to avoid offering upload in cases that are
        // likely to fail.
        if (!(StartsWith(verified_zip, zip, base::CompareCase::SENSITIVE) ||
              StartsWith(zip, verified_zip, base::CompareCase::SENSITIVE))) {
          upload_decision_metrics_ |=
              AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS;
          break;
        }
      }
    }
  }

  // If none of the candidate addresses have a zip, the candidate set is
  // invalid.
  if (verified_zip.empty() && !candidate_profiles.empty())
    upload_decision_metrics_ |= AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE;

  // Set up |upload_request->profiles|.
  upload_request->profiles.assign(candidate_profiles.begin(),
                                  candidate_profiles.end());
  if (!has_modified_profile) {
    for (const AutofillProfile& profile : candidate_profiles) {
      UMA_HISTOGRAM_COUNTS_1000(
          "Autofill.DaysSincePreviousUseAtSubmission.Profile",
          (profile.use_date() - profile.previous_use_date()).InDays());
    }
  }
}

int CreditCardSaveManager::GetDetectedValues() const {
  int detected_values = 0;

  // Report detecting CVC if it was found.
  if (!upload_request_.cvc.empty()) {
    detected_values |= DetectedValue::CVC;
  }

  // If cardholder name exists, set it as detected as long as
  // UPLOAD_NOT_OFFERED_CONFLICTING_NAMES was not set.
  if (!upload_request_.card
           .GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), app_locale_)
           .empty() &&
      !(upload_decision_metrics_ &
        AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES)) {
    detected_values |= DetectedValue::CARDHOLDER_NAME;
  }

  // Go through the upload request's profiles and set the following as detected:
  //  - ADDRESS_NAME, as long as UPLOAD_NOT_OFFERED_CONFLICTING_NAMES was not
  //    set
  //  - POSTAL_CODE, as long as UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS was not set
  //  - Any other address fields found on any addresses, regardless of conflicts
  for (const AutofillProfile& profile : upload_request_.profiles) {
    if (!profile.GetInfo(NAME_FULL, app_locale_).empty() &&
        !(upload_decision_metrics_ &
          AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES)) {
      detected_values |= DetectedValue::ADDRESS_NAME;
    }
    if (!profile.GetInfo(ADDRESS_HOME_ZIP, app_locale_).empty() &&
        !(upload_decision_metrics_ &
          AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS)) {
      detected_values |= DetectedValue::POSTAL_CODE;
    }
    if (!profile.GetInfo(ADDRESS_HOME_LINE1, app_locale_).empty()) {
      detected_values |= DetectedValue::ADDRESS_LINE;
    }
    if (!profile.GetInfo(ADDRESS_HOME_CITY, app_locale_).empty()) {
      detected_values |= DetectedValue::LOCALITY;
    }
    if (!profile.GetInfo(ADDRESS_HOME_STATE, app_locale_).empty()) {
      detected_values |= DetectedValue::ADMINISTRATIVE_AREA;
    }
    if (!profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty()) {
      detected_values |= DetectedValue::COUNTRY_CODE;
    }
  }

  // If the billing_customer_number is non-zero, it means the user has a Google
  // Payments account. Include a bit for existence of this account (NOT the id
  // itself), as it will help determine if a new Payments customer might need to
  // be created when save is accepted.
  if (payments::GetBillingCustomerId(personal_data_manager_,
                                     payments_client_->GetPrefService()) != 0) {
    detected_values |= DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;
  }

  // If one of the following is true, signal that cardholder name will be
  // explicitly requested in the offer-to-save bubble:
  //  1) Name is conflicting/missing, and the user does NOT have a Google
  //     Payments account
  //  2) The AutofillUpstreamAlwaysRequestCardholderName experiment is enabled
  //     (should only ever be used by testers, never launched)
  if ((!(detected_values & DetectedValue::CARDHOLDER_NAME) &&
       !(detected_values & DetectedValue::ADDRESS_NAME) &&
       !(detected_values & DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT) &&
       features::IsAutofillUpstreamEditableCardholderNameExperimentEnabled()) ||
      features::
          IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled()) {
    detected_values |= DetectedValue::USER_PROVIDED_NAME;
  }

  return detected_values;
}

void CreditCardSaveManager::OnUserDidAcceptUpload(
    const base::string16& cardholder_name) {
  // If cardholder name was explicitly requested for the user to enter/confirm,
  // replace the name on |upload_request_.card| with the entered name.  (Note
  // that it is possible a name already existed on the card if conflicting names
  // were found, which this intentionally overwrites.)
  if (!cardholder_name.empty()) {
    DCHECK(should_request_name_from_user_);
    upload_request_.card.SetInfo(CREDIT_CARD_NAME_FULL, cardholder_name,
                                 app_locale_);
  }
  user_did_accept_upload_prompt_ = true;
  // Populating risk data and offering upload occur asynchronously.
  // If |risk_data| has already been loaded, send the upload card request.
  // Otherwise, continue to wait and let OnDidGetUploadRiskData handle it.
  if (!upload_request_.risk_data.empty())
    SendUploadCardRequest();
}

void CreditCardSaveManager::OnDidGetUploadRiskData(
    const std::string& risk_data) {
  upload_request_.risk_data = risk_data;
  // Populating risk data and offering upload occur asynchronously.
  // If the dialog has already been accepted, send the upload card request.
  // Otherwise, continue to wait for the user to accept the save dialog.
  if (user_did_accept_upload_prompt_)
    SendUploadCardRequest();
}

void CreditCardSaveManager::SendUploadCardRequest() {
  if (observer_for_testing_)
    observer_for_testing_->OnSentUploadCardRequest();
  upload_request_.app_locale = app_locale_;
  upload_request_.billing_customer_number = payments::GetBillingCustomerId(
      personal_data_manager_, payments_client_->GetPrefService());

  AutofillMetrics::LogUploadAcceptedCardOriginMetric(
      uploading_local_card_
          ? AutofillMetrics::USER_ACCEPTED_UPLOAD_OF_LOCAL_CARD
          : AutofillMetrics::USER_ACCEPTED_UPLOAD_OF_NEW_CARD);
  payments_client_->UploadCard(
      upload_request_, base::BindOnce(&CreditCardSaveManager::OnDidUploadCard,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardSaveManager::OnStrikeChangeComplete(const int num_strikes) {
  if (observer_for_testing_)
    observer_for_testing_->OnCCSMStrikeChangeComplete();
}

AutofillMetrics::CardUploadDecisionMetric
CreditCardSaveManager::GetCVCCardUploadDecisionMetric() const {
  // This function assumes a valid CVC was not found.
  if (found_cvc_field_) {
    return found_value_in_cvc_field_ ? AutofillMetrics::INVALID_CVC_VALUE
                                     : AutofillMetrics::CVC_VALUE_NOT_FOUND;
  }
  return found_cvc_value_in_non_cvc_field_
             ? AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD
             : AutofillMetrics::CVC_FIELD_NOT_FOUND;
}

void CreditCardSaveManager::LogCardUploadDecisions(
    int upload_decision_metrics) {
  AutofillMetrics::LogCardUploadDecisionMetrics(upload_decision_metrics);
  AutofillMetrics::LogCardUploadDecisionsUkm(
      client_->GetUkmRecorder(), client_->GetUkmSourceId(),
      pending_upload_request_origin_.GetURL(), upload_decision_metrics);
  pending_upload_request_origin_ = url::Origin();
}

}  // namespace autofill
