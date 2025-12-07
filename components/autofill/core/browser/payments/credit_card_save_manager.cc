// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_save_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client_behavior_constants.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/autofill_save_card_ui_info.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strike_database/strike_database.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics_android.h"
#elif !BUILDFLAG(IS_IOS)
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics_desktop.h"
#endif

namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;
using SaveCardOfferUserDecision =
    payments::PaymentsAutofillClient::SaveCardOfferUserDecision;
using SaveCardPromptOffer = autofill_metrics::SaveCardPromptOffer;
using SaveCardPromptResult = autofill_metrics::SaveCardPromptResult;

constexpr bool is_ios = !!BUILDFLAG(IS_IOS);

// If |name| consists of three whitespace-separated parts and the second of the
// three parts is a single character or a single character followed by a period,
// returns the result of joining the first and third parts with a space.
// Otherwise, returns |name|.
//
// Note that a better way to do this would be to use SplitName from
// src/components/autofill/core/browser/data_model/addresses/contact_info.cc.
// However, for now we want the logic of which variations of names are
// considered to be the same to exactly match the logic applied on the Payments
// server.
std::u16string RemoveMiddleInitial(const std::u16string& name) {
  std::vector<std::u16string_view> parts =
      base::SplitStringPiece(name, base::kWhitespaceUTF16,
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 3 &&
      (parts[1].length() == 1 ||
       (parts[1].length() == 2 &&
        base::EndsWith(parts[1], u".", base::CompareCase::SENSITIVE)))) {
    parts.erase(parts.begin() + 1);
    return base::JoinString(parts, u" ");
  }
  return name;
}

// Prepares uploaded card for virtual card enrollment and returns details of
// enrollment response if uploaded card is eligible for virtual card
// enrollment.
std::optional<payments::GetDetailsForEnrollmentResponseDetails>
PrepareForVirtualCardEnroll(
    bool card_saved,
    payments::UploadCardResponseDetails upload_card_response_details,
    CreditCard* uploaded_card) {
  // `upload_card_response_details` has fields in the response that will be
  // required for server requests in the virtual card enrollment flow, so we set
  // them here and start the flow.
  if (card_saved &&
      upload_card_response_details.virtual_card_enrollment_state ==
          CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible) {
    DCHECK(upload_card_response_details.instrument_id.has_value());
    if (!upload_card_response_details.card_art_url.is_empty()) {
      uploaded_card->set_card_art_url(
          std::move(upload_card_response_details.card_art_url));
    }
    uploaded_card->set_virtual_card_enrollment_state(
        std::move(upload_card_response_details.virtual_card_enrollment_state));
    uploaded_card->set_instrument_id(
        upload_card_response_details.instrument_id.value());

    return upload_card_response_details
        .get_details_for_enrollment_response_details;
  }
  return std::nullopt;
}

#if BUILDFLAG(IS_IOS)
// Logs iOS-specific metrics for the save card prompt offer.
void LogSaveCardPromptOfferMetricIos(
    autofill_metrics::SaveCardPromptOffer metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions& options) {
  std::string_view destination = is_upload_save ? ".Server" : ".Local";

  std::string base_histogram_name =
      base::StrCat({"Autofill.SaveCreditCardPromptOffer.IOS", destination,
                    autofill::ShouldShowSaveCardBottomSheet(
                        options.card_save_type, options.num_strikes.value_or(0),
                        options.should_request_name_from_user,
                        options.should_request_expiration_date_from_user)
                        ? ".BottomSheet"
                        : ".Banner"});
  base::UmaHistogramEnumeration(base_histogram_name, metric);

  auto is_num_strikes_in_range = [](int strikes) {
    return strikes >= 0 && strikes <= 2;
  };

  // To avoid emitting an arbitrary number of histograms, limit
  // `num_strikes` to [0, 2], matching the save card's current maximum
  // allowed strikes.
  if (!options.num_strikes ||
      !is_num_strikes_in_range(*(options.num_strikes))) {
    return;
  }

  base::UmaHistogramEnumeration(
      base::StrCat({base_histogram_name, ".NumStrikes.",
                    base::NumberToString(options.num_strikes.value()),
                    (options.should_request_name_from_user &&
                     options.should_request_expiration_date_from_user)
                        ? ".RequestingCardHolderNameAndExpiryDate"
                    : (options.should_request_name_from_user)
                        ? ".RequestingCardHolderName"
                    : (options.should_request_expiration_date_from_user)
                        ? ".RequestingExpiryDate"
                        : ".NoFixFlow"}),
      metric);
}
#endif  // BUILDFLAG(IS_IOS)

// Logs metrics for whether the save card prompt is shown or not. When the
// prompt is not shown, it also logs platform-specific metrics since the save
// card flow does not proceed further.
void LogPromptOfferMetricForCreditCardSave(
    SaveCardPromptOffer metric,
    bool is_upload_save,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions& options =
        {}) {
  autofill_metrics::LogSaveCreditCardPromptOfferMetric(metric, is_upload_save);

  switch (metric) {
    case SaveCardPromptOffer::kNotShownMaxStrikesReached:
    case SaveCardPromptOffer::kCvcMissingForPotentialUpdate: {
#if BUILDFLAG(IS_ANDROID)
      autofill_metrics::LogSaveCreditCardPromptOfferMetricAndroid(
          metric, is_upload_save, /*save_credit_card_options=*/options);
#elif BUILDFLAG(IS_IOS)
      LogSaveCardPromptOfferMetricIos(metric, is_upload_save, options);
#else
      if (metric == SaveCardPromptOffer::kNotShownMaxStrikesReached) {
        // On desktop, save will be offered in the omnibox without popping-up
        // the bubble. Detailed metric will be logged by
        // SaveCardBubbleController when decision to show omnibox icon will be
        // taken.
        return;
      }
      autofill_metrics::LogSaveCreditCardPromptOfferMetricDesktop(
          metric, is_upload_save, /*save_credit_card_options=*/options);
#endif
      break;
    }
    case SaveCardPromptOffer::kShown:
    case SaveCardPromptOffer::kNotShownRequiredDelay:
      break;
  }
}
}  // namespace

CreditCardSaveManager::CreditCardSaveManager(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

CreditCardSaveManager::~CreditCardSaveManager() = default;

bool CreditCardSaveManager::AttemptToOfferCardLocalSave(
    const CreditCard& card) {
  if (!payments_autofill_client().LocalCardSaveIsSupported()) {
    return false;
  }
  card_save_candidate_ = card;
  show_save_prompt_.reset();

  // If the card data does not have the expiration month or the year, then do
  // not offer to save to save locally, as the local save bubble does not
  // support the expiration date fix flow.
  if (card_save_candidate_
          .GetInfo(CREDIT_CARD_EXP_MONTH, client_->GetAppLocale())
          .empty() ||
      card_save_candidate_
          .GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, client_->GetAppLocale())
          .empty()) {
    return false;
  }
  // Query the Autofill StrikeDatabase on if we should pop up the
  // offer-to-save prompt for this card.
  show_save_prompt_ = !GetCreditCardSaveStrikeDatabase()->ShouldBlockFeature(
      base::UTF16ToUTF8(card_save_candidate_.LastFourDigits()));
  OfferCardLocalSave();
  return show_save_prompt_.value_or(false);
}

bool CreditCardSaveManager::AttemptToOfferCvcLocalSave(const CreditCard& card) {
  card_save_candidate_ = card;
  show_save_prompt_.reset();

  show_save_prompt_ = !DetermineAndLogCvcSaveStrikeDatabaseBlockDecision();
  OfferCvcLocalSave();
  return show_save_prompt_.value();
}

bool CreditCardSaveManager::ShouldOfferCvcSave(
    const CreditCard& card,
    FormDataImporter::CreditCardImportType credit_card_import_type,
    bool is_credit_card_upstream_enabled) {
  // Only offer CVC save if CVC storage is enabled.
  if (!IsCvcSaveFlowAllowed()) {
    return false;
  }

  // Only offer CVC save if the user entered a CVC during checkout.
  if (card.cvc().empty()) {
    return false;
  }

  // We will only offer CVC-only save if the card is known to Autofill.
  const CreditCard* existing_credit_card = nullptr;
  switch (credit_card_import_type) {
    case FormDataImporter::CreditCardImportType::kLocalCard:
      existing_credit_card =
          payments_data_manager().GetCreditCardByGUID(card.guid());
      break;
    case FormDataImporter::CreditCardImportType::kDuplicateLocalServerCard:
      // Payments autofill shows the server card suggestion in the duplicate
      // case. Thus, set `exsting_credit_card` in the same way server cards are
      // set.
    case FormDataImporter::CreditCardImportType::kServerCard:
      // Offering CVC save for card info retrieval cards would be a bad user
      // experience because users would not be able to use the saved CVC, since
      // the card has a dynamic CVC that would be retrieved from the Payments
      // servers.
      if (is_credit_card_upstream_enabled &&
          card.card_info_retrieval_enrollment_state() !=
              CreditCard::CardInfoRetrievalEnrollmentState::
                  kRetrievalEnrolled) {
        existing_credit_card =
            payments_data_manager().GetCreditCardByInstrumentId(
                card.instrument_id());
      }
      break;
    case FormDataImporter::CreditCardImportType::kVirtualCard:
    case FormDataImporter::CreditCardImportType::kNoCard:
    case FormDataImporter::CreditCardImportType::kNewCard:
      break;
  }
  return existing_credit_card && existing_credit_card->cvc() != card.cvc();
}

bool CreditCardSaveManager::ProceedWithSavingIfApplicable(
    const FormStructure& submitted_form,
    const CreditCard& card,
    FormDataImporter::CreditCardImportType credit_card_import_type,
    bool is_credit_card_upstream_enabled,
    ukm::SourceId ukm_source_id) {
  // Prioritize card upload save if it is allowed. Check if card upload save
  // should be offer and attempt to offer card upload save. Card upload is only
  // offered if import_type is local card or new card. It can't be duplicate or
  // server card.
  if (is_credit_card_upstream_enabled &&
      (credit_card_import_type ==
           FormDataImporter::CreditCardImportType::kLocalCard ||
       credit_card_import_type ==
           FormDataImporter::CreditCardImportType::kNewCard)) {
    AttemptToOfferCardUploadSave(
        submitted_form, card,
        /*uploading_local_card=*/credit_card_import_type ==
            FormDataImporter::CreditCardImportType::kLocalCard,
        ukm_source_id);
    return true;
  }

  // If card upload is not allowed, we check if CVC save should be offer and
  // attempt to offer CVC save.
  if (!card.cvc().empty() && IsCvcSaveFlowAllowed()) {
    // We will only offer CVC-only save if the card is known to Autofill.
    const CreditCard* existing_credit_card = nullptr;
    if (card.record_type() == CreditCard::RecordType::kLocalCard) {
      existing_credit_card =
          payments_data_manager().GetCreditCardByGUID(card.guid());
      if (existing_credit_card && existing_credit_card->cvc() != card.cvc()) {
        AttemptToOfferCvcLocalSave(card);
        return true;
      }
    } else if (card.record_type() ==
                   CreditCard::RecordType::kMaskedServerCard &&
               is_credit_card_upstream_enabled) {
      existing_credit_card =
          payments_data_manager().GetCreditCardByInstrumentId(
              card.instrument_id());
      if (existing_credit_card && existing_credit_card->cvc() != card.cvc()) {
        AttemptToOfferCvcUploadSave(card);
        return true;
      }
    }
  }

  // If card upload save and CVC save are not allowed, new cards should be saved
  // locally.
  if (credit_card_import_type ==
      FormDataImporter::CreditCardImportType::kNewCard) {
    return AttemptToOfferCardLocalSave(card);
  }
  return false;
}

void CreditCardSaveManager::AttemptToOfferCardUploadSave(
    const FormStructure& submitted_form,
    const CreditCard& card,
    const bool uploading_local_card,
    ukm::SourceId ukm_source_id) {
  payments::PaymentsNetworkInterface* payments_network_interface =
      payments_autofill_client().GetPaymentsNetworkInterface();
  // Abort the uploading if `payments_network_interface` is nullptr.
  if (!payments_network_interface) {
    return;
  }
  upload_request_ = payments::UploadCardRequestDetails();
  upload_request_.card = card;
  uploading_local_card_ = uploading_local_card;
  show_save_prompt_.reset();

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
    const std::u16string& value = field->value_for_import();
    const bool is_valid_cvc =
        IsValidCreditCardSecurityCode(value, upload_request_.card.network());
    if (field->Type().GetCreditCardType() == CREDIT_CARD_VERIFICATION_CODE) {
      found_cvc_field_ = true;
      if (!value.empty()) {
        found_value_in_cvc_field_ = true;
      }
      if (is_valid_cvc) {
        upload_request_.cvc = value;
        break;
      }
    } else if (is_valid_cvc &&
               field->Type().GetTypes().contains(UNKNOWN_TYPE)) {
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

  // If `USER_MUST_PROVIDE_NAME` is present, cardholder name is
  // conflicting/missing and user didn't have Google Payments account. On iOS,
  // `USER_MUST_PROVIDE_NAME` is present if valid cardholder name was not
  // provided even if the user has a Google Payments account.
  if (upload_request_.detected_values & DetectedValue::USER_MUST_PROVIDE_NAME) {
    upload_decision_metrics_ |=
        autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME;
    should_request_name_from_user_ = true;
  }

  // If the user must provide expiration month or expiration year, log it and
  // set |should_request_expiration_date_from_user_| so the offer-to-save dialog
  // knows to ask for it.
  should_request_expiration_date_from_user_ = false;
  if (upload_request_.detected_values &
      DetectedValue::USER_MUST_PROVIDE_EXPIRATION_DATE) {
    upload_decision_metrics_ |=
        autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE;
    LogSaveCardRequestExpirationDateReasonMetric();
    should_request_expiration_date_from_user_ = true;
  }

  // The cardholder name and expiration date fix flows cannot both be
  // active at the same time, except on iOS, where the combined fix flow is
  // supported. If they are, abort offering upload. If user is signed in and has
  // Wallet Sync Transport enabled but we still need to request expiration date
  // from them, offering upload should be aborted as well.
#if !BUILDFLAG(IS_IOS)
  if ((should_request_name_from_user_ &&
       should_request_expiration_date_from_user_) ||
      (should_request_expiration_date_from_user_ &&
       payments_data_manager().IsPaymentsWalletSyncTransportEnabled())) {
    LogCardUploadDecisions(ukm_source_id, upload_decision_metrics_);
    return;
  }
#endif

  // If the card's last four digits matches the last four of an existing server
  // card but with a different expiration date, there's a chance this could be a
  // card update instead of new card upload. In those cases, CVC is required, so
  // abort offering upload if CVC is missing. (We should confirm first that
  // `upload_request_.card` actually has a full expiration date.)
  std::vector<const CreditCard*> server_cards =
      payments_data_manager().GetServerCreditCards();
  bool found_server_card_with_same_last_four_but_different_expiration =
      upload_request_.detected_values & DetectedValue::CARD_EXPIRATION_MONTH &&
      upload_request_.detected_values & DetectedValue::CARD_EXPIRATION_YEAR &&
      std::ranges::any_of(server_cards, [&](const CreditCard* server_card) {
        return server_card->HasSameNumberAs(upload_request_.card) &&
               !server_card->HasSameExpirationDateAs(upload_request_.card);
      });
  if (found_server_card_with_same_last_four_but_different_expiration &&
      upload_request_.cvc.empty()) {
    LogPromptOfferMetricForCreditCardSave(
        SaveCardPromptOffer::kCvcMissingForPotentialUpdate,
        /*is_upload_save=*/true,
        payments::PaymentsAutofillClient::SaveCreditCardOptions()
            .with_should_request_name_from_user(should_request_name_from_user_)
            .with_should_request_expiration_date_from_user(false)
            .with_same_last_four_as_server_card_but_different_expiration_date(
                true)
            .with_num_strikes(GetCreditCardSaveStrikeDatabase()->GetStrikes(
                base::UTF16ToUTF8(upload_request_.card.LastFourDigits())))
            .with_card_save_type(
                payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly));

    autofill_metrics::LogSaveCardPromptOfferMetric(
        SaveCardPromptOffer::kCvcMissingForPotentialUpdate,
        /*is_upload_save=*/true, /*is_reshow=*/false,
        payments::PaymentsAutofillClient::SaveCreditCardOptions()
            .with_should_request_name_from_user(should_request_name_from_user_)
            .with_should_request_expiration_date_from_user(false)
            .with_same_last_four_as_server_card_but_different_expiration_date(
                true)
            .with_num_strikes(GetCreditCardSaveStrikeDatabase()->GetStrikes(
                base::UTF16ToUTF8(upload_request_.card.LastFourDigits())))
            .with_card_save_type(
                payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly),
        payments_data_manager().GetPaymentsSigninStateForMetrics());
    LogCardUploadDecisions(ukm_source_id, upload_decision_metrics_);
    return;
  }

  // Only send the country of the recently-used addresses. We make a copy here
  // to avoid modifying |upload_request_.profiles|, which should always have
  // full addresses even after this function goes out of scope.
  std::vector<AutofillProfile> country_only_profiles;
  for (const AutofillProfile& address : upload_request_.profiles) {
    const std::u16string country_code =
        address.GetRawInfo(ADDRESS_HOME_COUNTRY);
    AutofillProfile country_only(
        AddressCountryCode(base::UTF16ToUTF8(country_code)));
    country_only_profiles.emplace_back(std::move(country_only));
  }

  // All required data is available, start the upload process.
  if (observer_for_testing_) {
    observer_for_testing_->OnDecideToRequestUploadSave();
  }

  // Query the Autofill StrikeDatabase on if we should pop up the
  // offer-to-save prompt for this card.
  show_save_prompt_ = !GetCreditCardSaveStrikeDatabase()->ShouldBlockFeature(
      base::UTF16ToUTF8(upload_request_.card.LastFourDigits()));

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  upload_request_.client_behavior_signals.push_back(
      ClientBehaviorConstants::kShowAccountEmailInLegalMessage);
#endif

  // Check if we should request the CVC-inclusive legal message and if the user
  // has enabled CVC storage.
  if (ShouldRequestCvcInclusiveLegalMessage() && IsCvcSaveFlowAllowed()) {
    upload_request_.client_behavior_signals.push_back(
        ClientBehaviorConstants::kOfferingToSaveCvc);
  }

  payments_network_interface->GetCardUploadDetails(
      country_only_profiles, upload_request_.detected_values,
      upload_request_.client_behavior_signals, client_->GetAppLocale(),
      base::BindOnce(&CreditCardSaveManager::OnDidGetUploadDetails,
                     weak_ptr_factory_.GetWeakPtr(), ukm_source_id),
      payments::kUploadPaymentMethodBillableServiceNumber,
      payments::GetBillingCustomerId(payments_data_manager()),
      payments::UploadCardSource::kUpstreamCheckoutFlow);
}

void CreditCardSaveManager::AttemptToOfferCvcUploadSave(
    const CreditCard& card) {
  card_save_candidate_ = card;
  show_save_prompt_.reset();

  show_save_prompt_ = !DetermineAndLogCvcSaveStrikeDatabaseBlockDecision();

  if (!is_ios || show_save_prompt_.value_or(true)) {
    // TODO(crbug.com/40931101): Refactor ShowSaveCreditCardToCloud to change
    // legal_message_lines_ to optional.
    payments_autofill_client().ShowSaveCreditCardToCloud(
        card_save_candidate_, legal_message_lines_,
        payments::PaymentsAutofillClient::SaveCreditCardOptions()
            .with_show_prompt(show_save_prompt_.value())
            .with_card_save_type(
                payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly),
        base::BindOnce(&CreditCardSaveManager::OnUserDidDecideOnCvcUploadSave,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool CreditCardSaveManager::IsCreditCardUploadEnabled() {
#if BUILDFLAG(IS_IOS)
  // If observer_for_testing_ is set, assume we are in a browsertest and
  // credit card upload should be enabled by default.
  // TODO(crbug.com/40583419): Remove dependency from iOS tests on this
  // behavior.
  if (observer_for_testing_) {
    return true;
  }
#endif  // BUILDFLAG(IS_IOS)
  return ::autofill::IsCreditCardUploadEnabled(
      client_->GetSyncService(), *client_->GetPrefs(),
      payments_data_manager().GetCountryCodeForExperimentGroup(),
      payments_data_manager().GetPaymentsSigninStateForMetrics(),
      client_->GetCurrentLogManager());
}

void CreditCardSaveManager::OnDidUploadCard(
    PaymentsRpcResult result,
    const payments::UploadCardResponseDetails& upload_card_response_details) {
  if (observer_for_testing_) {
    observer_for_testing_->OnReceivedUploadCardResponse();
  }

  if (result == PaymentsRpcResult::kSuccess) {
    // Log how many strikes the card had when it was saved.
    LogStrikesPresentWhenCardSaved(
        /*is_local=*/false,
        GetCreditCardSaveStrikeDatabase()->GetStrikes(
            base::UTF16ToUTF8(upload_request_.card.LastFourDigits())));

    // Clear all CreditCardSave strikes for this card, in case it is later
    // removed.
    GetCreditCardSaveStrikeDatabase()->ClearStrikes(
        base::UTF16ToUTF8(upload_request_.card.LastFourDigits()));

    if (!upload_request_.card.cvc().empty() &&
        upload_card_response_details.instrument_id.has_value() &&
        IsCvcSaveFlowAllowed()) {
      // After a card is successfully saved to server, if CVC storage is
      // enabled, save server CVC to PaymentsAutofillTable if it exists.
      payments_data_manager().AddServerCvc(
          upload_card_response_details.instrument_id.value(),
          upload_request_.card.cvc());
    }
  } else {
    // If the upload failed, fallback to a local card save if supported.
    // Do not save if card does not have the expiration month or the year
    // because the local save bubble does not support the expiration date fix
    // flow.
    bool run_save_card_fallback =
        payments_autofill_client().LocalCardSaveIsSupported();

    if (run_save_card_fallback &&
        !upload_request_.card
             .GetInfo(CREDIT_CARD_EXP_MONTH, client_->GetAppLocale())
             .empty() &&
        !upload_request_.card
             .GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, client_->GetAppLocale())
             .empty()) {
      autofill_metrics::LogCreditCardUploadRanLocalSaveFallbackMetric(
          /*new_local_card_added=*/payments_data_manager().SaveCardLocallyIfNew(
              upload_request_.card));
    }

    // If the upload failed and the bubble was actually shown (NOT just the
    // icon), count that as a strike against offering upload in the future.
    if (show_save_prompt_.has_value() && show_save_prompt_.value()) {
      int nth_strike_added = GetCreditCardSaveStrikeDatabase()->AddStrike(
          base::UTF16ToUTF8(upload_request_.card.LastFourDigits()));
      // Notify the browsertests that a strike was added.
      OnStrikeChangeComplete(nth_strike_added);
    }
  }

  // Prepare for virtual card enrollment if uploaded card is eligible for
  // virtual card enrollment.
  std::optional<payments::GetDetailsForEnrollmentResponseDetails>
      get_details_for_enrollment_response_details = PrepareForVirtualCardEnroll(
          /*card_saved=*/result == PaymentsRpcResult::kSuccess,
          std::move(upload_card_response_details),
          /*uploaded_card=*/&upload_request_.card);

  auto on_confirmation_closed_callback =
      get_details_for_enrollment_response_details.has_value()
          ? std::make_optional(base::BindOnce(
                &CreditCardSaveManager::InitVirtualCardEnroll,
                weak_ptr_factory_.GetWeakPtr(), upload_request_.card,
                std::move(get_details_for_enrollment_response_details)))
          : std::nullopt;

  // Show credit card upload feedback.
  payments_autofill_client().CreditCardUploadCompleted(
      result, std::move(on_confirmation_closed_callback));

  if (observer_for_testing_) {
    observer_for_testing_->OnShowCardSavedFeedback();
  }
}

void CreditCardSaveManager::InitVirtualCardEnroll(
    const CreditCard& credit_card,
    std::optional<payments::GetDetailsForEnrollmentResponseDetails>
        get_details_for_enrollment_response_details) {
  // Hides save card confirmation dialog if still showing.
  payments_autofill_client().HideSaveCardPrompt();

  if (auto* virtual_card_enrollment_manager =
          payments_autofill_client().GetVirtualCardEnrollmentManager()) {
    virtual_card_enrollment_manager->InitVirtualCardEnroll(
        credit_card, VirtualCardEnrollmentSource::kUpstream,
        base::BindOnce(
            &VirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble,
            base::Unretained(virtual_card_enrollment_manager)),
        std::move(get_details_for_enrollment_response_details));
  }
}

CreditCardSaveStrikeDatabase*
CreditCardSaveManager::GetCreditCardSaveStrikeDatabase() const {
  if (credit_card_save_strike_database_.get() == nullptr) {
    credit_card_save_strike_database_ =
        std::make_unique<CreditCardSaveStrikeDatabase>(
            CreditCardSaveStrikeDatabase(client_->GetStrikeDatabase()));
  }
  return credit_card_save_strike_database_.get();
}

CvcStorageStrikeDatabase* CreditCardSaveManager::GetCvcStorageStrikeDatabase() {
  if (!client_->GetStrikeDatabase()) {
    return nullptr;
  }
  if (!cvc_storage_strike_database_) {
    cvc_storage_strike_database_ = std::make_unique<CvcStorageStrikeDatabase>(
        CvcStorageStrikeDatabase(client_->GetStrikeDatabase()));
  }
  return cvc_storage_strike_database_.get();
}

bool CreditCardSaveManager::
    DetermineAndLogCvcSaveStrikeDatabaseBlockDecision() {
  auto* cvc_storage_strike_db = GetCvcStorageStrikeDatabase();
  CHECK(cvc_storage_strike_db);
  bool is_upload_save = card_save_candidate_.record_type() ==
                        CreditCard::RecordType::kMaskedServerCard;
  std::string id =
      is_upload_save
          ? base::NumberToString(card_save_candidate_.instrument_id())
          : card_save_candidate_.guid();

  CvcStorageStrikeDatabase::StrikeDatabaseDecision decision =
      cvc_storage_strike_db->GetStrikeDatabaseDecision(id);

  switch (decision) {
    case CvcStorageStrikeDatabase::kDoNotBlock:
      return false;
    case CvcStorageStrikeDatabase::kMaxStrikeLimitReached:
      autofill_metrics::LogSaveCvcPromptOfferMetric(
          SaveCardPromptOffer::kNotShownMaxStrikesReached, is_upload_save,
          /*is_reshow=*/false);
      return true;
    case CvcStorageStrikeDatabase::kRequiredDelayNotPassed:
      autofill_metrics::LogSaveCvcPromptOfferMetric(
          SaveCardPromptOffer::kNotShownRequiredDelay, is_upload_save,
          /*is_reshow=*/false);
      return true;
  }
}

void CreditCardSaveManager::OnDidGetUploadDetails(
    ukm::SourceId ukm_source_id,
    PaymentsRpcResult result,
    const std::u16string& context_token,
    std::unique_ptr<base::Value::Dict> legal_message,
    std::vector<std::pair<int, int>> supported_card_bin_ranges) {
  if (observer_for_testing_) {
    observer_for_testing_->OnReceivedGetUploadDetailsResponse();
  }
  if (result == PaymentsRpcResult::kSuccess) {
    LegalMessageLine::Parse(*legal_message, &legal_message_lines_,
                            /*escape_apostrophes=*/true);

    if (legal_message_lines_.empty()) {
      // Parsing legal messages failed, so upload should not be offered.
      // Offer local card save if card is not already saved locally.
      if (!uploading_local_card_) {
        AttemptToOfferCardLocalSave(upload_request_.card);
      }
      upload_decision_metrics_ |=
          autofill_metrics::UPLOAD_NOT_OFFERED_INVALID_LEGAL_MESSAGE;
      LogCardUploadDecisions(ukm_source_id, upload_decision_metrics_);
      return;
    }

    // Do *not* call
    // `payments_autofill_client().GetPaymentsNetworkInterface()->Prepare()`
    // here. We shouldn't send credentials until the user has explicitly
    // accepted a prompt to upload.
    if (!supported_card_bin_ranges.empty() &&
        !payments::IsCreditCardNumberSupported(upload_request_.card.number(),
                                               supported_card_bin_ranges)) {
      // Attempt local card save if card not already saved.
      if (!uploading_local_card_) {
        AttemptToOfferCardLocalSave(upload_request_.card);
      }
      upload_decision_metrics_ |=
          autofill_metrics::UPLOAD_NOT_OFFERED_UNSUPPORTED_BIN_RANGE;
      LogCardUploadDecisions(ukm_source_id, upload_decision_metrics_);
      return;
    }
    upload_request_.context_token = context_token;
    OfferCardUploadSave(ukm_source_id);
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
    if (found_name_and_postal_code_and_cvc && !uploading_local_card_) {
      AttemptToOfferCardLocalSave(upload_request_.card);
    }
    upload_decision_metrics_ |=
        autofill_metrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED;
    LogCardUploadDecisions(ukm_source_id, upload_decision_metrics_);
  }
}

void CreditCardSaveManager::OfferCardLocalSave() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  bool is_mobile_build = true;
#else
  bool is_mobile_build = false;
#endif  // #if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  payments::PaymentsAutofillClient::CardSaveType card_save_type =
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly;
  // Show `kCardSaveWithCvc` prompt if flag is on and CVC is not empty.
  if (!card_save_candidate_.cvc().empty() && IsCvcSaveFlowAllowed()) {
    card_save_type =
        payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc;
  }

  payments::PaymentsAutofillClient::SaveCreditCardOptions options =
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          // TODO(crbug.com/40280819): Refactor SaveCreditCardOptions.
          .with_show_prompt(show_save_prompt_.value_or(true))
          .with_num_strikes(GetCreditCardSaveStrikeDatabase()->GetStrikes(
              base::UTF16ToUTF8(card_save_candidate_.LastFourDigits())))
          .with_card_save_type(card_save_type);

  // If |show_save_prompt_|'s value is false, desktop builds will still offer
  // save in the omnibox without popping-up the bubble. Mobile builds,
  // however, should not display the offer-to-save infobar at all.
  if (!is_mobile_build || show_save_prompt_.value_or(true)) {
    if (observer_for_testing_) {
      observer_for_testing_->OnOfferLocalSave();
    }

    payments_autofill_client().ShowSaveCreditCardLocally(
        card_save_candidate_, options,
        base::BindOnce(&CreditCardSaveManager::OnUserDidDecideOnLocalSave,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  if (show_save_prompt_.has_value()) {
    if (show_save_prompt_.value()) {
      LogPromptOfferMetricForCreditCardSave(SaveCardPromptOffer::kShown,
                                            /*is_upload_save=*/false);
    } else if (!show_save_prompt_.value()) {
      autofill_metrics::LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
          AutofillMetrics::SaveTypeMetric::LOCAL);
      LogPromptOfferMetricForCreditCardSave(
          SaveCardPromptOffer::kNotShownMaxStrikesReached,
          /*is_upload_save=*/false, options);
    }
  }
}

void CreditCardSaveManager::OfferCvcLocalSave() {
  if (!is_ios || show_save_prompt_.value_or(true)) {
    payments_autofill_client().ShowSaveCreditCardLocally(
        card_save_candidate_,
        payments::PaymentsAutofillClient::SaveCreditCardOptions()
            .with_show_prompt(show_save_prompt_.value_or(false))
            .with_card_save_type(
                payments::PaymentsAutofillClient::CardSaveType::kCvcSaveOnly),
        base::BindOnce(&CreditCardSaveManager::OnUserDidDecideOnCvcLocalSave,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CreditCardSaveManager::OfferCardUploadSave(ukm::SourceId ukm_source_id) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  bool is_mobile_build = true;
#else
  bool is_mobile_build = false;
#endif  // #if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

  payments::PaymentsAutofillClient::CardSaveType card_save_type =
      payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly;
  // Show `kCardSaveWithCvc` prompt if flag is on and CVC is not empty.
  if (!upload_request_.card.cvc().empty() && IsCvcSaveFlowAllowed()) {
    card_save_type =
        payments::PaymentsAutofillClient::CardSaveType::kCardSaveWithCvc;
  }

  std::vector<const CreditCard*> server_cards =
      payments_data_manager().GetServerCreditCards();
  // At this point of the flow, we know there are no masked server cards with
  // the same last four digits and expiration date as the card we are
  // attempting to save, since if there were any we would have matched it and
  // not be saving this card.
  bool found_server_card_with_same_last_four_but_different_expiration =
      std::ranges::any_of(server_cards, [&](const CreditCard* server_card) {
        return server_card->HasSameNumberAs(upload_request_.card) &&
               !server_card->HasSameExpirationDateAs(upload_request_.card);
      });

  payments::PaymentsAutofillClient::SaveCreditCardOptions options =
      payments::PaymentsAutofillClient::SaveCreditCardOptions()
          .with_has_multiple_legal_lines(legal_message_lines_.size() > 1)
          .with_should_request_name_from_user(should_request_name_from_user_)
          .with_should_request_expiration_date_from_user(
              should_request_expiration_date_from_user_)
          .with_show_prompt(show_save_prompt_.value_or(true))
          .with_same_last_four_as_server_card_but_different_expiration_date(
              found_server_card_with_same_last_four_but_different_expiration)
          .with_num_strikes(GetCreditCardSaveStrikeDatabase()->GetStrikes(
              base::UTF16ToUTF8(upload_request_.card.LastFourDigits())))
          .with_card_save_type(card_save_type);

  // If |show_save_prompt_|'s value is false, desktop builds will still offer
  // save in the omnibox without popping-up the bubble. Mobile builds, however,
  // should not display the offer-to-save infobar at all.
  if (!is_mobile_build || show_save_prompt_.value_or(true)) {
    user_did_accept_upload_prompt_ = false;
    if (observer_for_testing_) {
      observer_for_testing_->OnOfferUploadSave();
    }
    payments_autofill_client().ShowSaveCreditCardToCloud(
        upload_request_.card, legal_message_lines_, options,
        base::BindOnce(&CreditCardSaveManager::OnUserDidDecideOnUploadSave,
                       weak_ptr_factory_.GetWeakPtr()));
    payments_autofill_client().LoadRiskData(
        base::BindOnce(&CreditCardSaveManager::OnDidGetUploadRiskData,
                       weak_ptr_factory_.GetWeakPtr()));

    // Log metrics.
    AutofillMetrics::LogUploadOfferedCardOriginMetric(
        uploading_local_card_ ? AutofillMetrics::OFFERING_UPLOAD_OF_LOCAL_CARD
                              : AutofillMetrics::OFFERING_UPLOAD_OF_NEW_CARD);

    // Set that upload was offered.
    upload_decision_metrics_ |= autofill_metrics::UPLOAD_OFFERED;
  } else {
    // Set that upload was abandoned due to the Autofill StrikeDatabase
    // returning too many strikes for a mobile infobar to be displayed.
    upload_decision_metrics_ |=
        autofill_metrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE;
  }
  LogCardUploadDecisions(ukm_source_id, upload_decision_metrics_);
  if (show_save_prompt_.has_value()) {
    if (show_save_prompt_.value()) {
      LogPromptOfferMetricForCreditCardSave(SaveCardPromptOffer::kShown,
                                            /*is_upload_save=*/true);
    } else if (!show_save_prompt_.value()) {
      autofill_metrics::LogCreditCardSaveNotOfferedDueToMaxStrikesMetric(
          AutofillMetrics::SaveTypeMetric::SERVER);
      LogPromptOfferMetricForCreditCardSave(
          SaveCardPromptOffer::kNotShownMaxStrikesReached,
          /*is_upload_save=*/true, options);
    }
  }
}

void CreditCardSaveManager::OnUserDidDecideOnLocalSave(
    SaveCardOfferUserDecision user_decision) {
  switch (user_decision) {
    case SaveCardOfferUserDecision::kAccepted:
      autofill_metrics::LogSaveCreditCardPromptResultMetric(
          SaveCardPromptResult::kAccepted, /*is_upload_save=*/false);
      // Log how many CreditCardSave strikes the card had when it was saved.
      LogStrikesPresentWhenCardSaved(
          /*is_local=*/true,
          GetCreditCardSaveStrikeDatabase()->GetStrikes(
              base::UTF16ToUTF8(card_save_candidate_.LastFourDigits())));
      // Clear all CreditCardSave strikes for this card, in case it is later
      // removed.
      GetCreditCardSaveStrikeDatabase()->ClearStrikes(
          base::UTF16ToUTF8(card_save_candidate_.LastFourDigits()));

      // Clear the CVC value from the `card_save_candidate_` if CVC storage
      // isn't enabled.
      if (!card_save_candidate_.cvc().empty() && !IsCvcSaveFlowAllowed()) {
        card_save_candidate_.clear_cvc();
      }

      payments_data_manager().OnAcceptedLocalCreditCardSave(
          card_save_candidate_);
      break;
    case SaveCardOfferUserDecision::kDeclined:
    case SaveCardOfferUserDecision::kIgnored:
      autofill_metrics::LogSaveCreditCardPromptResultMetric(
          SaveCardPromptResult::kClosed, /*is_upload_save=*/false);
      OnUserDidIgnoreOrDeclineSave(card_save_candidate_.LastFourDigits());
      break;
  }
}

void CreditCardSaveManager::OnUserDidDecideOnCvcLocalSave(
    SaveCardOfferUserDecision user_decision) {
  switch (user_decision) {
    case SaveCardOfferUserDecision::kAccepted:
      // If accepted, clear all CvcStorage strikes for this CVC, in case the CVC
      // is later removed and we want to offer local CVC save for this card
      // again.
      if (auto* cvc_storage_strike_db = GetCvcStorageStrikeDatabase()) {
        cvc_storage_strike_db->ClearStrikes(card_save_candidate_.guid());
      }
      payments_data_manager().UpdateLocalCvc(card_save_candidate_.guid(),
                                             card_save_candidate_.cvc());
      break;
    case SaveCardOfferUserDecision::kDeclined:
      // If the user rejected save and the offer-to-save bubble, treat
      // that as a final strike and block this feature.
      if (auto* cvc_storage_strike_db = GetCvcStorageStrikeDatabase()) {
        int strike_difference =
            cvc_storage_strike_db->GetMaxStrikesLimit() -
            cvc_storage_strike_db->GetStrikes(card_save_candidate_.guid());
        if (strike_difference > 0) {
          cvc_storage_strike_db->AddStrikes(
              /*strikes_increase=*/strike_difference,
              /*id=*/card_save_candidate_.guid());
        }
      }
      break;
    case SaveCardOfferUserDecision::kIgnored:
      if (show_save_prompt_.value_or(false) && GetCvcStorageStrikeDatabase()) {
        // If the user ignored save and the offer-to-save bubble or
        // infobar was actually shown (NOT just the icon if on desktop), count
        // that as a strike against offering upload in the future.
        GetCvcStorageStrikeDatabase()->AddStrike(card_save_candidate_.guid());
      }
  }
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
    payments::UploadCardRequestDetails* upload_request) {
  std::vector<AutofillProfile> candidate_profiles;
  const base::Time now = AutofillClock::Now();
  const base::TimeDelta fifteen_minutes = base::Minutes(15);
  // Reset |upload_decision_metrics_| to begin logging detected problems.
  upload_decision_metrics_ = 0;
  bool has_profile = false;

  // First, process address profiles that have been preliminarily imported.
  for (const AutofillProfile& profile :
       preliminarily_imported_address_profiles_) {
    has_profile = true;
    candidate_profiles.push_back(profile);
  }

  // Second, collect all of the already stored addresses used or modified
  // recently.
  for (const AutofillProfile* profile :
       client_->GetPersonalDataManager().address_data_manager().GetProfiles()) {
    has_profile = true;
    if ((now - profile->usage_history().use_date()) < fifteen_minutes ||
        (now - profile->usage_history().modification_date()) <
            fifteen_minutes) {
      candidate_profiles.push_back(*profile);
    }
  }

  if (candidate_profiles.empty()) {
    upload_decision_metrics_ |=
        has_profile
            ? autofill_metrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS
            : autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE;
  }

  // If any of the names on the card or the addresses don't match the
  // candidate set is invalid. This matches the rules for name matching applied
  // server-side by Google Payments and ensures that we don't send upload
  // requests that are guaranteed to fail.
  const std::u16string card_name =
      card.GetInfo(CREDIT_CARD_NAME_FULL, client_->GetAppLocale());
  std::u16string verified_name;
  if (candidate_profiles.empty()) {
    verified_name = card_name;
  } else {
    bool found_conflicting_names = false;
    verified_name = RemoveMiddleInitial(card_name);
    for (const AutofillProfile& profile : candidate_profiles) {
      const std::u16string address_name = RemoveMiddleInitial(
          profile.GetInfo(NAME_FULL, client_->GetAppLocale()));
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
          autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES;
    }
  }

  // If neither the card nor any of the addresses have a name associated with
  // them, the candidate set is invalid.
  if (verified_name.empty()) {
    upload_decision_metrics_ |= autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME;
  }

  // If any of the candidate addresses have a non-empty zip that doesn't match
  // any other non-empty zip, then the candidate set is invalid.
  std::u16string verified_zip;
  for (const AutofillProfile& profile : candidate_profiles) {
    const std::u16string zip = profile.GetRawInfo(ADDRESS_HOME_ZIP);
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
              autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS;
          break;
        }
      }
    }
  }

  // If none of the candidate addresses have a zip, the candidate set is
  // invalid.
  if (verified_zip.empty() && !candidate_profiles.empty())
    upload_decision_metrics_ |=
        autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE;

  // Set up |upload_request->profiles|.
  upload_request->profiles.assign(candidate_profiles.begin(),
                                  candidate_profiles.end());
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
           .GetInfo(CREDIT_CARD_NAME_FULL, client_->GetAppLocale())
           .empty() &&
      !(upload_decision_metrics_ &
        autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES)) {
    detected_values |= DetectedValue::CARDHOLDER_NAME;
  }

  // Go through the upload request's profiles and set the following as detected:
  //  - ADDRESS_NAME, as long as UPLOAD_NOT_OFFERED_CONFLICTING_NAMES was not
  //    set
  //  - POSTAL_CODE, as long as UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS was not set
  //  - Any other address fields found on any addresses, regardless of conflicts
  for (const AutofillProfile& profile : upload_request_.profiles) {
    if (!profile.GetInfo(NAME_FULL, client_->GetAppLocale()).empty() &&
        !(upload_decision_metrics_ &
          autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES)) {
      detected_values |= DetectedValue::ADDRESS_NAME;
    }
    if (!profile.GetInfo(ADDRESS_HOME_ZIP, client_->GetAppLocale()).empty() &&
        !(upload_decision_metrics_ &
          autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS)) {
      detected_values |= DetectedValue::POSTAL_CODE;
    }
    if (!profile.GetInfo(ADDRESS_HOME_LINE1, client_->GetAppLocale()).empty()) {
      detected_values |= DetectedValue::ADDRESS_LINE;
    }
    if (!profile.GetInfo(ADDRESS_HOME_CITY, client_->GetAppLocale()).empty()) {
      detected_values |= DetectedValue::LOCALITY;
    }
    if (!profile.GetInfo(ADDRESS_HOME_STATE, client_->GetAppLocale()).empty()) {
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
  if (payments::HasGooglePaymentsAccount(payments_data_manager())) {
    detected_values |= DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;
  }

  // If expiration date month or expiration year are missing, signal that
  // expiration date will be explicitly requested in the offer-to-save bubble.
  if (!upload_request_.card
           .GetInfo(CREDIT_CARD_EXP_MONTH, client_->GetAppLocale())
           .empty()) {
    detected_values |= DetectedValue::CARD_EXPIRATION_MONTH;
  }
  if (!(upload_request_.card
            .GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, client_->GetAppLocale())
            .empty())) {
    detected_values |= DetectedValue::CARD_EXPIRATION_YEAR;
  }

  // Set |USER_MUST_PROVIDE_EXPIRATION_DATE| if expiration date is detected as
  // expired or missing.
  if (detected_values & DetectedValue::CARD_EXPIRATION_MONTH &&
      detected_values & DetectedValue::CARD_EXPIRATION_YEAR) {
    int month_value = 0, year_value = 0;
    bool parsable =
        base::StringToInt(upload_request_.card.GetInfo(CREDIT_CARD_EXP_MONTH,
                                                       client_->GetAppLocale()),
                          &month_value) &&
        base::StringToInt(
            upload_request_.card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                         client_->GetAppLocale()),
            &year_value);
    DCHECK(parsable);
    if (!IsValidCreditCardExpirationDate(year_value, month_value,
                                         AutofillClock::Now())) {
      detected_values |= DetectedValue::USER_MUST_PROVIDE_EXPIRATION_DATE;
    }
  } else {
    detected_values |= DetectedValue::USER_MUST_PROVIDE_EXPIRATION_DATE;
  }

  // If cardholder name is conflicting/missing and the user does NOT have a
  // Google Payments account, signal that cardholder name will be explicitly
  // requested in the offer-to-save bubble.
  if (!(detected_values & DetectedValue::CARDHOLDER_NAME) &&
      !(detected_values & DetectedValue::ADDRESS_NAME) &&
      !(detected_values & DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT)) {
    detected_values |= DetectedValue::USER_MUST_PROVIDE_NAME;
  }

#if BUILDFLAG(IS_IOS)
  // On iOS, a valid cardholder name is required and should be requested if
  // missing, even if the user already has a Google Payments account.
  if (!(detected_values & DetectedValue::CARDHOLDER_NAME)) {
    detected_values |= DetectedValue::USER_MUST_PROVIDE_NAME;
  }
#endif  // BUILDFLAG(IS_IOS)

  return detected_values;
}

void CreditCardSaveManager::OnUserDidDecideOnUploadSave(
    SaveCardOfferUserDecision user_decision,
    const payments::PaymentsAutofillClient::UserProvidedCardDetails&
        user_provided_card_details) {
  switch (user_decision) {
    case SaveCardOfferUserDecision::kAccepted:
      autofill_metrics::LogSaveCreditCardPromptResultMetric(
          SaveCardPromptResult::kAccepted, /*is_upload_save=*/true);
#if BUILDFLAG(IS_ANDROID)
      // On Android, requesting cardholder name is a two step flow.
      if (should_request_name_from_user_) {
        payments_autofill_client().ConfirmAccountNameFixFlow(base::BindOnce(
            &CreditCardSaveManager::OnUserDidAcceptAccountNameFixFlow,
            weak_ptr_factory_.GetWeakPtr()));
        // On Android, requesting expiration date is a two step flow.
      } else if (should_request_expiration_date_from_user_) {
        payments_autofill_client().ConfirmExpirationDateFixFlow(
            upload_request_.card,
            base::BindOnce(
                &CreditCardSaveManager::OnUserDidAcceptExpirationDateFixFlow,
                weak_ptr_factory_.GetWeakPtr()));
      } else {
        OnUserDidAcceptUploadHelper(user_provided_card_details);
      }
#else
      OnUserDidAcceptUploadHelper(user_provided_card_details);
#endif  // BUILDFLAG(IS_ANDROID)
      break;
    case SaveCardOfferUserDecision::kDeclined:
    case SaveCardOfferUserDecision::kIgnored:
      autofill_metrics::LogSaveCreditCardPromptResultMetric(
          SaveCardPromptResult::kClosed, /*is_upload_save=*/true);
      OnUserDidIgnoreOrDeclineSave(upload_request_.card.LastFourDigits());
      break;
  }

  payments_data_manager().OnUserAcceptedUpstreamOffer();
}

void CreditCardSaveManager::OnUserDidDecideOnCvcUploadSave(
    SaveCardOfferUserDecision user_decision,
    const payments::PaymentsAutofillClient::UserProvidedCardDetails&
        user_provided_card_details) {
  switch (user_decision) {
    case SaveCardOfferUserDecision::kAccepted: {
      // If accepted, clear all CvcStorage strikes for this CVC, in case the CVC
      // is later removed and we want to offer upload CVC save for this card
      // again.
      if (auto* cvc_storage_strike_db = GetCvcStorageStrikeDatabase()) {
        cvc_storage_strike_db->ClearStrikes(
            base::NumberToString(card_save_candidate_.instrument_id()));
      }
      CHECK(card_save_candidate_.instrument_id());
      if (const CreditCard* old_credit_card =
              payments_data_manager().GetCreditCardByInstrumentId(
                  card_save_candidate_.instrument_id())) {
        CHECK(old_credit_card->cvc() != card_save_candidate_.cvc());
        // If existing card doesn't have CVC, we insert CVC into
        // `kServerStoredCvcTable` table. If the existing card does have CVC, we
        // update CVC for `kServerStoredCvcTable` table.
        if (old_credit_card->cvc().empty()) {
          payments_data_manager().AddServerCvc(
              card_save_candidate_.instrument_id(), card_save_candidate_.cvc());
        } else {
          payments_data_manager().UpdateServerCvc(
              card_save_candidate_.instrument_id(), card_save_candidate_.cvc());
        }
      }
      break;
    }
    case SaveCardOfferUserDecision::kDeclined:
      // If the user rejected save and the offer-to-save bubble, treat
      // that as a final strike and block this feature.
      if (auto* cvc_storage_strike_db = GetCvcStorageStrikeDatabase()) {
        int strike_difference =
            cvc_storage_strike_db->GetMaxStrikesLimit() -
            cvc_storage_strike_db->GetStrikes(
                base::NumberToString(card_save_candidate_.instrument_id()));
        if (strike_difference > 0) {
          cvc_storage_strike_db->AddStrikes(
              /*strikes_increase=*/strike_difference,
              /*id=*/base::NumberToString(
                  card_save_candidate_.instrument_id()));
        }
      }
      break;
    case SaveCardOfferUserDecision::kIgnored:
      if (show_save_prompt_.value_or(false) && GetCvcStorageStrikeDatabase()) {
        // If the user ignored save and the offer-to-save bubble or
        // infobar was actually shown (NOT just the icon if on desktop), count
        // that as a strike against offering upload in the future.
        GetCvcStorageStrikeDatabase()->AddStrike(
            base::NumberToString(card_save_candidate_.instrument_id()));
      }
  }
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
void CreditCardSaveManager::OnUserDidAcceptAccountNameFixFlow(
    const std::u16string& cardholder_name) {
  DCHECK(should_request_name_from_user_);

  payments::PaymentsAutofillClient::UserProvidedCardDetails details;
  details.cardholder_name = cardholder_name;
  OnUserDidAcceptUploadHelper(details);
}

void CreditCardSaveManager::OnUserDidAcceptExpirationDateFixFlow(
    const std::u16string& month,
    const std::u16string& year) {
  payments::PaymentsAutofillClient::UserProvidedCardDetails details;
  details.expiration_date_month = month;
  details.expiration_date_year = year;
  OnUserDidAcceptUploadHelper(details);
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

void CreditCardSaveManager::OnUserDidAcceptUploadHelper(
    const payments::PaymentsAutofillClient::UserProvidedCardDetails&
        user_provided_card_details) {
  // If cardholder name was explicitly requested for the user to enter/confirm,
  // replace the name on |upload_request_.card| with the entered name.  (Note
  // that it is possible a name already existed on the card if conflicting names
  // were found, which this intentionally overwrites.)
  if (!user_provided_card_details.cardholder_name.empty()) {
    // On iOS, the cardholder name was provided by the user, but not through the
    // fix flow triggered via |should_request_name_from_user_|.
#if !BUILDFLAG(IS_IOS)
    DCHECK(should_request_name_from_user_);
#endif
    upload_request_.card.SetInfo(CREDIT_CARD_NAME_FULL,
                                 user_provided_card_details.cardholder_name,
                                 client_->GetAppLocale());
  }

  user_did_accept_upload_prompt_ = true;
  // If expiration date was explicitly requested for the user to select, replace
  // the expiration date on |upload_request_.card| with the selected date.
  if (!user_provided_card_details.expiration_date_month.empty() &&
      !user_provided_card_details.expiration_date_year.empty()) {
    // On iOS the expiration date was provided by the user, but not through the
    // fix flow triggered via |should_request_expiration_date_from_user_|.
#if !BUILDFLAG(IS_IOS)
    DCHECK(should_request_expiration_date_from_user_);
#endif
    upload_request_.card.SetInfo(
        CREDIT_CARD_EXP_MONTH, user_provided_card_details.expiration_date_month,
        client_->GetAppLocale());
    upload_request_.card.SetInfo(
        CREDIT_CARD_EXP_4_DIGIT_YEAR,
        user_provided_card_details.expiration_date_year,
        client_->GetAppLocale());
  }

// On iOS, the user can add a CVC on the save card details page. This CVC is
// then passed here and set on the card to be uploaded. For other platforms,
// this is a no-op as `user_provided_card_details.cvc` will be empty.
#if BUILDFLAG(IS_IOS)
  if (!user_provided_card_details.cvc.empty()) {
    upload_request_.card.set_cvc(user_provided_card_details.cvc);
  }
#endif
  // Virtual card enrollment manager may not be set of CWV clients.
  if (auto* virtual_card_enrollment_manager =
          payments_autofill_client().GetVirtualCardEnrollmentManager()) {
    virtual_card_enrollment_manager
      ->SetSaveCardBubbleAcceptedTimestamp(AutofillClock::Now());
  }

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
  if (observer_for_testing_) {
    observer_for_testing_->OnSentUploadCardRequest();
  }
  upload_request_.app_locale = client_->GetAppLocale();
  upload_request_.billing_customer_number =
      payments::GetBillingCustomerId(payments_data_manager());

  AutofillMetrics::LogUploadAcceptedCardOriginMetric(
      uploading_local_card_
          ? AutofillMetrics::USER_ACCEPTED_UPLOAD_OF_LOCAL_CARD
          : AutofillMetrics::USER_ACCEPTED_UPLOAD_OF_NEW_CARD);
  client_->GetPaymentsAutofillClient()
      ->GetPaymentsNetworkInterface()
      ->UploadCard(upload_request_,
                   base::BindOnce(&CreditCardSaveManager::OnDidUploadCard,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardSaveManager::OnUserDidIgnoreOrDeclineSave(
    const std::u16string& card_last_four_digits) {
  if (show_save_prompt_.has_value() && show_save_prompt_.value()) {
    // If the user rejected or ignored save and the offer-to-save bubble or
    // infobar was actually shown (NOT just the icon if on desktop), count
    // that as a strike against offering upload in the future.
    int nth_strike_added = GetCreditCardSaveStrikeDatabase()->AddStrike(
        base::UTF16ToUTF8(card_last_four_digits));
    OnStrikeChangeComplete(nth_strike_added);
  }
}

void CreditCardSaveManager::OnStrikeChangeComplete(const int num_strikes) {
  if (observer_for_testing_) {
    observer_for_testing_->OnStrikeChangeComplete();
  }
}

autofill_metrics::CardUploadDecision
CreditCardSaveManager::GetCVCCardUploadDecisionMetric() const {
  // This function assumes a valid CVC was not found.
  if (found_cvc_field_) {
    return found_value_in_cvc_field_ ? autofill_metrics::INVALID_CVC_VALUE
                                     : autofill_metrics::CVC_VALUE_NOT_FOUND;
  }
  return found_cvc_value_in_non_cvc_field_
             ? autofill_metrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD
             : autofill_metrics::CVC_FIELD_NOT_FOUND;
}

void CreditCardSaveManager::LogCardUploadDecisions(
    ukm::SourceId ukm_source_id,
    int upload_decision_metrics) {
  autofill_metrics::LogCardUploadDecisionMetrics(upload_decision_metrics);
  autofill_metrics::LogCardUploadDecisionsUkm(
      client_->GetUkmRecorder(), ukm_source_id,
      pending_upload_request_origin_.GetURL(), upload_decision_metrics);
  pending_upload_request_origin_ = url::Origin();
  LogCardUploadDecisionsToAutofillInternals(upload_decision_metrics);
}

void CreditCardSaveManager::LogCardUploadDecisionsToAutofillInternals(
    int upload_decision_metrics) {
  LogManager* log_manager = client_->GetCurrentLogManager();

  auto final_decision =
      (upload_decision_metrics_ & autofill_metrics::UPLOAD_OFFERED)
          ? LogMessage::kCardUploadDecisionUploadOffered
          : LogMessage::kCardUploadDecisionUploadNotOffered;

  LogBuffer buffer(IsLoggingActive(log_manager));
  LOG_AF(buffer) << LoggingScope::kCardUploadDecision << final_decision;
  LOG_AF(buffer) << Tag{"div"} << Attrib{"class", "form"} << Tag{"tr"}
                 << Tag{"td"} << "Decision Metrics:" << CTag{"td"} << Tag{"td"}
                 << Tag{"table"};

  for (int i = 0; i < autofill_metrics::kNumCardUploadDecisionMetrics; i++) {
    autofill_metrics::CardUploadDecision currentBitmaskValue =
        static_cast<autofill_metrics::CardUploadDecision>(1 << i);
    if (!(upload_decision_metrics & currentBitmaskValue))
      continue;

    std::string result;
    switch (currentBitmaskValue) {
      case autofill_metrics::UPLOAD_OFFERED:
        result = "UPLOAD_OFFERED";
        break;
      case autofill_metrics::CVC_FIELD_NOT_FOUND:
        result = "CVC_FIELD_NOT_FOUND";
        break;
      case autofill_metrics::CVC_VALUE_NOT_FOUND:
        result = "CVC_VALUE_NOT_FOUND";
        break;
      case autofill_metrics::INVALID_CVC_VALUE:
        result = "INVALID_CVC_VALUE";
        break;
      case autofill_metrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD:
        result = "FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE:
        result = "UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS:
        result = "UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE:
        result = "UPLOAD_NOT_OFFERED_NO_ZIP_CODE";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS:
        result = "UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_NO_NAME:
        result = "UPLOAD_NOT_OFFERED_NO_NAME";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES:
        result = "UPLOAD_NOT_OFFERED_CONFLICTING_NAMES";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED:
        result = "UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED";
        break;
      case autofill_metrics::USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME:
        result = "USER_REQUESTED_TO_PROVIDE_CARDHOLDER_NAME";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE:
        result = "UPLOAD_NOT_OFFERED_MAX_STRIKES_ON_MOBILE";
        break;
      case autofill_metrics::USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE:
        result = "USER_REQUESTED_TO_PROVIDE_EXPIRATION_DATE";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_UNSUPPORTED_BIN_RANGE:
        result = "UPLOAD_NOT_OFFERED_UNSUPPORTED_BIN_RANGE";
        break;
      case autofill_metrics::UPLOAD_NOT_OFFERED_INVALID_LEGAL_MESSAGE:
        result = "UPLOAD_NOT_OFFERED_INVALID_LEGAL_MESSAGE";
        break;
    }
    LOG_AF(buffer) << Tr{} << result;
  }
  LOG_AF(buffer) << CTag{"table"} << CTag{"td"} << CTag{"tr"} << CTag{"div"};
  LOG_AF(log_manager) << std::move(buffer);
}

void CreditCardSaveManager::LogSaveCardRequestExpirationDateReasonMetric() {
  bool is_month_empty =
      upload_request_.card
          .GetInfo(CREDIT_CARD_EXP_MONTH, client_->GetAppLocale())
          .empty();
  bool is_year_empty =
      upload_request_.card
          .GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, client_->GetAppLocale())
          .empty();

  if (is_month_empty && is_year_empty) {
    autofill_metrics::LogSaveCardRequestExpirationDateReasonMetric(
        autofill_metrics::SaveCardRequestExpirationDateReason::
            kMonthAndYearMissing);
  } else if (is_month_empty) {
    autofill_metrics::LogSaveCardRequestExpirationDateReasonMetric(
        autofill_metrics::SaveCardRequestExpirationDateReason::
            kMonthMissingOnly);
  } else if (is_year_empty) {
    autofill_metrics::LogSaveCardRequestExpirationDateReasonMetric(
        autofill_metrics::SaveCardRequestExpirationDateReason::
            kYearMissingOnly);
  } else {
    int month = 0, year = 0;
    bool parsable =
        base::StringToInt(
            upload_request_.card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                         client_->GetAppLocale()),
            &year) &&
        base::StringToInt(upload_request_.card.GetInfo(CREDIT_CARD_EXP_MONTH,
                                                       client_->GetAppLocale()),
                          &month);
    DCHECK(parsable);
    // Month and year are not empty, so they must be expired.
    DCHECK(!IsValidCreditCardExpirationDate(year, month, AutofillClock::Now()));
    autofill_metrics::LogSaveCardRequestExpirationDateReasonMetric(
        autofill_metrics::SaveCardRequestExpirationDateReason::
            kExpirationDatePresentButExpired);
  }
}

bool CreditCardSaveManager::ShouldRequestCvcInclusiveLegalMessage() const {
  // If the main CVC storage feature is disabled, we should never request the
  // CVC-inclusive legal message.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableCvcStorageAndFilling)) {
    return false;
  }
#if BUILDFLAG(IS_IOS)
  // On iOS, we request the CVC-inclusive message if a CVC is already present,
  // or if the save prompt will be the infobar and detail page flow, where a CVC
  // can be added by the user.
  if (!upload_request_.card.cvc().empty()) {
    return true;
  }

  int num_strikes = GetCreditCardSaveStrikeDatabase()->GetStrikes(
      base::UTF16ToUTF8(upload_request_.card.LastFourDigits()));
  // Since this code is only reached when no CVC was found on the form,
  // the save type is kCardSaveOnly.
  return !autofill::ShouldShowSaveCardBottomSheet(
             payments::PaymentsAutofillClient::CardSaveType::kCardSaveOnly,
             num_strikes, should_request_name_from_user_,
             should_request_expiration_date_from_user_) ||
         !base::FeatureList::IsEnabled(features::kAutofillSaveCardBottomSheet);
#else
  // For other platforms, we only request the CVC-inclusive message if a CVC
  // was present in the form.
  return !upload_request_.card.cvc().empty();
#endif  // BUILDFLAG(IS_IOS)
}

bool CreditCardSaveManager::IsCvcSaveFlowAllowed() const {
  return client_->IsCvcSavingSupported() &&
         payments_data_manager().IsPaymentCvcStorageEnabled();
}

PaymentsDataManager& CreditCardSaveManager::payments_data_manager() {
  return const_cast<PaymentsDataManager&>(
      const_cast<const CreditCardSaveManager*>(this)->payments_data_manager());
}

const PaymentsDataManager& CreditCardSaveManager::payments_data_manager()
    const {
  return client_->GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill
