// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/virtual_card_enrollment_strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/autofill/core/browser/strike_databases/strike_database_base.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace autofill {
namespace {

using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

}  // namespace

VirtualCardEnrollmentFields::VirtualCardEnrollmentFields() = default;
VirtualCardEnrollmentFields::VirtualCardEnrollmentFields(
    const VirtualCardEnrollmentFields&) = default;
VirtualCardEnrollmentFields& VirtualCardEnrollmentFields::operator=(
    const VirtualCardEnrollmentFields&) = default;
VirtualCardEnrollmentFields::~VirtualCardEnrollmentFields() = default;
bool VirtualCardEnrollmentFields::operator==(
    const VirtualCardEnrollmentFields&) const = default;

VirtualCardEnrollmentProcessState::VirtualCardEnrollmentProcessState() =
    default;
VirtualCardEnrollmentProcessState::VirtualCardEnrollmentProcessState(
    const VirtualCardEnrollmentProcessState&) = default;
VirtualCardEnrollmentProcessState& VirtualCardEnrollmentProcessState::operator=(
    const VirtualCardEnrollmentProcessState&) = default;

VirtualCardEnrollmentProcessState::~VirtualCardEnrollmentProcessState() =
    default;

VirtualCardEnrollmentManager::VirtualCardEnrollmentManager(
    PersonalDataManager* personal_data_manager,
    payments::PaymentsNetworkInterface* payments_network_interface,
    AutofillClient* autofill_client)
    : autofill_client_(autofill_client),
      personal_data_manager_(personal_data_manager),
      payments_network_interface_(payments_network_interface) {
  // |autofill_client_| does not exist on Clank settings page where this flow
  // can also be triggered.
  if (autofill_client_ && autofill_client_->GetStrikeDatabase()) {
    virtual_card_enrollment_strike_database_ =
        std::make_unique<VirtualCardEnrollmentStrikeDatabase>(
            autofill_client_->GetStrikeDatabase());
  }
}

VirtualCardEnrollmentManager::~VirtualCardEnrollmentManager() = default;

void VirtualCardEnrollmentManager::InitVirtualCardEnroll(
    const CreditCard& credit_card,
    VirtualCardEnrollmentSource virtual_card_enrollment_source,
    std::optional<payments::PaymentsNetworkInterface::
                      GetDetailsForEnrollmentResponseDetails>
        get_details_for_enrollment_response_details,
    PrefService* user_prefs,
    RiskAssessmentFunction risk_assessment_function,
    VirtualCardEnrollmentFieldsLoadedCallback
        virtual_card_enrollment_fields_loaded_callback) {
  // If at strike limit, exit enrollment flow.
  if (ShouldBlockVirtualCardEnrollment(
          base::NumberToString(credit_card.instrument_id()),
          virtual_card_enrollment_source)) {
    Reset();
    return;
  }

  SetInitialVirtualCardEnrollFields(credit_card,
                                    virtual_card_enrollment_source);

  if (get_details_for_enrollment_response_details.has_value() &&
      IsValidGetDetailsForEnrollmentResponseDetails(
          get_details_for_enrollment_response_details.value())) {
    SetGetDetailsForEnrollmentResponseDetails(
        get_details_for_enrollment_response_details.value());
  }

  // |autofill_client_| being nullptr denotes that we are in the Clank settings
  // page virtual card enrollment use case, so we will need to use
  // |risk_assessment_function_| to load risk data as we do not have access to
  // web contents, and |virtual_card_enrollment_fields_loaded_callback_| to
  // display the UI.
  if (!autofill_client_) {
    risk_assessment_function_ = std::move(risk_assessment_function);
    virtual_card_enrollment_fields_loaded_callback_ =
        std::move(virtual_card_enrollment_fields_loaded_callback);
  }

  LoadRiskDataAndContinueFlow(
      user_prefs,
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnRiskDataLoadedForVirtualCard,
          weak_ptr_factory_.GetWeakPtr()));
}

void VirtualCardEnrollmentManager::Enroll(
    std::optional<VirtualCardEnrollmentUpdateResponseCallback>
        virtual_card_enrollment_update_response_callback) {
  LogUpdateVirtualCardEnrollmentRequestAttempt(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      VirtualCardEnrollmentRequestType::kEnroll);
  payments::PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails
      request_details;
  request_details.virtual_card_enrollment_source =
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source;
  request_details.virtual_card_enrollment_request_type =
      VirtualCardEnrollmentRequestType::kEnroll;
  request_details.billing_customer_number = payments::GetBillingCustomerId(
      &personal_data_manager_->payments_data_manager());
  request_details.instrument_id =
      state_.virtual_card_enrollment_fields.credit_card.instrument_id();
  request_details.vcn_context_token = state_.vcn_context_token;

  virtual_card_enrollment_update_response_callback_ =
      std::move(virtual_card_enrollment_update_response_callback);

  payments_network_interface_->UpdateVirtualCardEnrollment(
      request_details,
      base::BindOnce(&VirtualCardEnrollmentManager::
                         OnDidGetUpdateVirtualCardEnrollmentResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     VirtualCardEnrollmentRequestType::kEnroll));
    RemoveAllStrikesToBlockOfferingVirtualCardEnrollment(base::NumberToString(
        state_.virtual_card_enrollment_fields.credit_card.instrument_id()));
}

void VirtualCardEnrollmentManager::Unenroll(
    int64_t instrument_id,
    std::optional<VirtualCardEnrollmentUpdateResponseCallback>
        virtual_card_enrollment_update_response_callback) {
  LogUpdateVirtualCardEnrollmentRequestAttempt(
      VirtualCardEnrollmentSource::kSettingsPage,
      VirtualCardEnrollmentRequestType::kUnenroll);

  payments::PaymentsNetworkInterface::UpdateVirtualCardEnrollmentRequestDetails
      request_details;
  state_.virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kSettingsPage;

  // Unenroll can only happen from the settings page.
  request_details.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kSettingsPage;

  request_details.virtual_card_enrollment_request_type =
      VirtualCardEnrollmentRequestType::kUnenroll;
  request_details.billing_customer_number = payments::GetBillingCustomerId(
      &personal_data_manager_->payments_data_manager());
  request_details.instrument_id = instrument_id;

  virtual_card_enrollment_update_response_callback_ =
      std::move(virtual_card_enrollment_update_response_callback);

  payments_network_interface_->UpdateVirtualCardEnrollment(
      request_details,
      base::BindOnce(&VirtualCardEnrollmentManager::
                         OnDidGetUpdateVirtualCardEnrollmentResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     VirtualCardEnrollmentRequestType::kUnenroll));
}

bool VirtualCardEnrollmentManager::ShouldBlockVirtualCardEnrollment(
    const std::string& instrument_id,
    VirtualCardEnrollmentSource virtual_card_enrollment_source) const {
  if (virtual_card_enrollment_source ==
      VirtualCardEnrollmentSource::kSettingsPage) {
    return false;
  }

  if (!GetVirtualCardEnrollmentStrikeDatabase())
    return false;

  VirtualCardEnrollmentStrikeDatabase::StrikeDatabaseDecision decision =
      GetVirtualCardEnrollmentStrikeDatabase()->GetStrikeDatabaseDecision(
          instrument_id);
  switch (decision) {
    case VirtualCardEnrollmentStrikeDatabase::kDoNotBlock:
      return false;
    case VirtualCardEnrollmentStrikeDatabase::kMaxStrikeLimitReached:
      LogVirtualCardEnrollmentBubbleMaxStrikesLimitReached(
          virtual_card_enrollment_source);
      LogVirtualCardEnrollmentNotOfferedDueToMaxStrikes(
          virtual_card_enrollment_source);
      return true;
    case VirtualCardEnrollmentStrikeDatabase::kRequiredDelayNotPassed:
      LogVirtualCardEnrollmentNotOfferedDueToRequiredDelay(
          virtual_card_enrollment_source);
      return true;
  }
}

void VirtualCardEnrollmentManager::
    AddStrikeToBlockOfferingVirtualCardEnrollment(
        const std::string& instrument_id) {
  if (!GetVirtualCardEnrollmentStrikeDatabase())
    return;

  GetVirtualCardEnrollmentStrikeDatabase()->AddStrike(instrument_id);

  // Log that a strike has been recorded.
  LogVirtualCardEnrollmentStrikeDatabaseEvent(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      VirtualCardEnrollmentStrikeDatabaseEvent::
          VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKE_LOGGED);
}

void VirtualCardEnrollmentManager::
    RemoveAllStrikesToBlockOfferingVirtualCardEnrollment(
        const std::string& instrument_id) {
  if (!GetVirtualCardEnrollmentStrikeDatabase())
    return;

  // Before we remove the existing strikes for the card, log the strike number
  // first.
  base::UmaHistogramCounts1000(
      "Autofill.StrikeDatabase.StrikesPresentWhenVirtualCardEnrolled",
      GetVirtualCardEnrollmentStrikeDatabase()->GetStrikes(instrument_id));

  GetVirtualCardEnrollmentStrikeDatabase()->ClearStrikes(instrument_id);

  // Log that strikes are being cleared.
  LogVirtualCardEnrollmentStrikeDatabaseEvent(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      VirtualCardEnrollmentStrikeDatabaseEvent::
          VIRTUAL_CARD_ENROLLMENT_STRIKE_DATABASE_STRIKES_CLEARED);
}

void VirtualCardEnrollmentManager::SetSaveCardBubbleAcceptedTimestamp(
    base::Time save_card_bubble_accepted_timestamp) {
  save_card_bubble_accepted_timestamp_ =
      std::move(save_card_bubble_accepted_timestamp);
}

void VirtualCardEnrollmentManager::ClearAllStrikesForTesting() {
  GetVirtualCardEnrollmentStrikeDatabase()->ClearAllStrikes();
}

void VirtualCardEnrollmentManager::OnDidGetUpdateVirtualCardEnrollmentResponse(
    VirtualCardEnrollmentRequestType type,
    PaymentsRpcResult result) {
  // Add a strike if enrollment attempt was not successful.
  if (type == VirtualCardEnrollmentRequestType::kEnroll &&
      result != PaymentsRpcResult::kSuccess) {
    AddStrikeToBlockOfferingVirtualCardEnrollment(base::NumberToString(
        state_.virtual_card_enrollment_fields.credit_card.instrument_id()));
  }

  // Relay the response to the server card editor page. This also destroys the
  // payments delegate if the editor was already closed.
  if (virtual_card_enrollment_update_response_callback_.has_value()) {
    std::move(virtual_card_enrollment_update_response_callback_.value())
        .Run(result);
  }

  LogUpdateVirtualCardEnrollmentRequestResult(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      type, result == PaymentsRpcResult::kSuccess);
  Reset();
}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollCompleted(
    PaymentsRpcResult result) {
  autofill_client_->GetPaymentsAutofillClient()->VirtualCardEnrollCompleted(
      result);
}

void VirtualCardEnrollmentManager::Reset() {
  payments_network_interface_->CancelRequest();
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_ = VirtualCardEnrollmentProcessState();
  enroll_response_details_received_ = false;
  virtual_card_enrollment_update_response_callback_.reset();
}

VirtualCardEnrollmentStrikeDatabase*
VirtualCardEnrollmentManager::GetVirtualCardEnrollmentStrikeDatabase() const {
  return virtual_card_enrollment_strike_database_.get();
}

void VirtualCardEnrollmentManager::LoadRiskDataAndContinueFlow(
    PrefService* user_prefs,
    base::OnceCallback<void(const std::string&)> callback) {
  if (autofill_client_) {
    autofill_client_->GetPaymentsAutofillClient()->LoadRiskData(
        std::move(callback));
  } else {
    // No |autofill_client_| present indicates we are in the clank settings page
    // use case, so we load risk data using a method that does not require web
    // contents to be present.
    std::move(risk_assessment_function_)
        .Run(/*obfuscated_gaia_id=*/0, user_prefs, std::move(callback),
             /*web_contents=*/nullptr, gfx::Rect());
  }
}

void VirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble() {
  DCHECK(autofill_client_);
  if (state_.virtual_card_enrollment_fields.virtual_card_enrollment_source ==
          VirtualCardEnrollmentSource::kUpstream &&
      save_card_bubble_accepted_timestamp_.has_value()) {
    LogVirtualCardEnrollBubbleLatencySinceUpstream(
        AutofillClock::Now() - save_card_bubble_accepted_timestamp_.value());
    save_card_bubble_accepted_timestamp_.reset();
  }

  // Check in StrikeDatabase whether enrollment has been offered for this card
  // and got declined before and whether this is the last time this offer is
  // shown before previous records expire.
  state_.virtual_card_enrollment_fields.previously_declined = false;
  state_.virtual_card_enrollment_fields.last_show = false;
  if (GetVirtualCardEnrollmentStrikeDatabase()) {
    std::string card_instrument_id = base::NumberToString(
        state_.virtual_card_enrollment_fields.credit_card.instrument_id());
    if (GetVirtualCardEnrollmentStrikeDatabase()->GetStrikes(
            card_instrument_id) > 0) {
      state_.virtual_card_enrollment_fields.previously_declined = true;
    }
    if (GetVirtualCardEnrollmentStrikeDatabase()->IsLastOffer(
            card_instrument_id)) {
      state_.virtual_card_enrollment_fields.last_show = true;
    }
  }

  autofill_client_->GetPaymentsAutofillClient()->ShowVirtualCardEnrollDialog(
      state_.virtual_card_enrollment_fields,
      base::BindOnce(
          &VirtualCardEnrollmentManager::Enroll, weak_ptr_factory_.GetWeakPtr(),
          /*virtual_card_enrollment_update_response_callback=*/
          base::BindOnce(
              &VirtualCardEnrollmentManager::OnVirtualCardEnrollCompleted,
              weak_ptr_factory_.GetWeakPtr())),
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled,
          weak_ptr_factory_.GetWeakPtr()));
}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled() {
    AddStrikeToBlockOfferingVirtualCardEnrollment(base::NumberToString(
        state_.virtual_card_enrollment_fields.credit_card.instrument_id()));
  Reset();
}

void VirtualCardEnrollmentManager::OnRiskDataLoadedForVirtualCard(
    const std::string& risk_data) {
  state_.risk_data = risk_data;

  // If we are in the upstream case and the
  // GetDetailsForEnrollmentResponseDetails were already received, then we
  // received it from the UploadCardResponseDetails. Thus, we can skip making
  // another GetDetailsForEnrollmentRequest and go straight to showing the
  // bubble.
  if (state_.virtual_card_enrollment_fields.virtual_card_enrollment_source ==
          VirtualCardEnrollmentSource::kUpstream &&
      enroll_response_details_received_) {
    // We are about to show the virtual card enroll bubble, so make sure the
    // card art image is set to then display in the bubble.
    EnsureCardArtImageIsSetBeforeShowingUI();

    // Shows the virtual card enroll bubble.
    ShowVirtualCardEnrollBubble();
  } else {
    // We are not in the upstream case where we received the
    // GetDetailsForEnrollmentResponseDetails in the UploadCardResponseDetails,
    // so we need to make a GetDetailsForEnroll request.
    GetDetailsForEnroll();
  }
}

void VirtualCardEnrollmentManager::GetDetailsForEnroll() {
  payments::PaymentsNetworkInterface::GetDetailsForEnrollmentRequestDetails
      request_details;
  request_details.app_locale = personal_data_manager_->app_locale();
  request_details.risk_data = state_.risk_data.value_or("");
  request_details.billing_customer_number = payments::GetBillingCustomerId(
      &personal_data_manager_->payments_data_manager());
  request_details.instrument_id =
      state_.virtual_card_enrollment_fields.credit_card.instrument_id();
  request_details.source =
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source;

  get_details_for_enrollment_request_sent_timestamp_ = AutofillClock::Now();

  payments_network_interface_->GetVirtualCardEnrollmentDetails(
      request_details,
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnDidGetDetailsForEnrollResponse,
          weak_ptr_factory_.GetWeakPtr()));

  LogGetDetailsForEnrollmentRequestAttempt(request_details.source);
}

void VirtualCardEnrollmentManager::OnDidGetDetailsForEnrollResponse(
    PaymentsRpcResult result,
    const payments::PaymentsNetworkInterface::
        GetDetailsForEnrollmentResponseDetails& response) {
  if (get_details_for_enrollment_request_sent_timestamp_.has_value()) {
    LogGetDetailsForEnrollmentRequestLatency(
        state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
        result,
        AutofillClock::Now() -
            get_details_for_enrollment_request_sent_timestamp_.value());
    get_details_for_enrollment_request_sent_timestamp_.reset();
  }

  LogGetDetailsForEnrollmentRequestResult(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      /*succeeded=*/result == PaymentsRpcResult::kSuccess);

  // Show the virtual card permanent error dialog if server explicitly returned
  // permanent error, show temporary error dialog for the rest of the failure
  // cases since currently only virtual card is supported.
  if (result != PaymentsRpcResult::kSuccess) {
    // Showing an error dialog here would provide a confusing user experience as
    // it is an error for a flow that is not user-initiated, so we fail
    // silently.
    Reset();
    return;
  }

  // The response is already checked in
  // GetDetailsForEnrollmentRequest::IsResponseComplete(), so we should have a
  // valid GetDetailsForEnrollmentResponseDetails here.
  DCHECK(IsValidGetDetailsForEnrollmentResponseDetails(response));
  SetGetDetailsForEnrollmentResponseDetails(response);

  // We are about to show the UI for virtual card enrollment, so make sure the
  // card art image is set to then display in the bubble.
  EnsureCardArtImageIsSetBeforeShowingUI();

  if (autofill_client_) {
    ShowVirtualCardEnrollBubble();
  } else {
    // If the |autofill_client_| is not present, it means that the request is
    // from Android settings page, thus run the callback with the
    // |virtual_card_enrollment_fields_|, which would show the enrollment
    // dialog.
    std::move(virtual_card_enrollment_fields_loaded_callback_)
        .Run(&state_.virtual_card_enrollment_fields);
  }
}

void VirtualCardEnrollmentManager::SetGetDetailsForEnrollmentResponseDetails(
    const payments::PaymentsNetworkInterface::
        GetDetailsForEnrollmentResponseDetails& response) {
  enroll_response_details_received_ = true;
  state_.virtual_card_enrollment_fields.google_legal_message =
      std::move(response.google_legal_message);

  // Issuer legal message is empty for some issuers.
  if (!response.issuer_legal_message.empty()) {
    state_.virtual_card_enrollment_fields.issuer_legal_message =
        std::move(response.issuer_legal_message);
  }

  // The |vcn_context_token| will be used by the server to link the previous
  // GetDetailsForEnrollRequest to the future UpdateVirtualCardEnrollmentRequest
  // if the user decides to enroll |state_|'s |virtual_card_enrollment_fields|'s
  // |credit_card| as a virtual card.
  state_.vcn_context_token = response.vcn_context_token;
}

void VirtualCardEnrollmentManager::EnsureCardArtImageIsSetBeforeShowingUI() {
  if (state_.virtual_card_enrollment_fields.card_art_image)
    return;

  // Tries to get the card art image from the local cache. If the card art
  // image is not available, then we fall back to the network image instead.
  // The card art image might not be present in the upstream flow if the
  // chrome sync server has not synced down the card art url yet for the card
  // just uploaded.
  gfx::Image* cached_card_art_image =
      personal_data_manager_->payments_data_manager()
          .GetCachedCardArtImageForUrl(
              state_.virtual_card_enrollment_fields.credit_card.card_art_url());
  if (cached_card_art_image && !cached_card_art_image->IsEmpty()) {
    // We found a card art image in the cache, so set |state_|'s
    // |virtual_card_enrollment_fields|'s |card_art_image| to it.
    state_.virtual_card_enrollment_fields.card_art_image =
        cached_card_art_image->ToImageSkia();
  } else {
    // We did not find a card art image in the cache, so set |state_|'s
    // |virtual_card_enrollment_fields|'s |card_art_image| to the network
    // image instead.
    state_.virtual_card_enrollment_fields.card_art_image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            CreditCard::IconResourceId(
                state_.virtual_card_enrollment_fields.credit_card.network()));
  }
}

void VirtualCardEnrollmentManager::SetInitialVirtualCardEnrollFields(
    const CreditCard& credit_card,
    VirtualCardEnrollmentSource virtual_card_enrollment_source) {
  // Reset here to override currently pending enrollment.
  Reset();

  DCHECK_NE(virtual_card_enrollment_source, VirtualCardEnrollmentSource::kNone);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Hide the bubble and icon if it is already showing for a previous enrollment
  // bubble.
  DCHECK(autofill_client_);
  autofill_client_->GetPaymentsAutofillClient()
      ->HideVirtualCardEnrollBubbleAndIconIfVisible();
#endif

  state_.virtual_card_enrollment_fields.credit_card = credit_card;

  // The |card_art_image| might not be synced yet from the sync server which
  // will result in a nullptr. This situation can occur in the upstream flow.
  // If it is not synced, GetCreditCardArtImageForUrl() will send a fetch
  // request to sync the |card_art_image|, and before showing the
  // VirtualCardEnrollmentBubble we will try to fetch the |card_art_image|
  // from the local cache.
  gfx::Image* card_art_image =
      personal_data_manager_->payments_data_manager()
          .GetCreditCardArtImageForUrl(credit_card.card_art_url());
  if (card_art_image && !card_art_image->IsEmpty()) {
    state_.virtual_card_enrollment_fields.card_art_image =
        card_art_image->ToImageSkia();
  }

  state_.virtual_card_enrollment_fields.virtual_card_enrollment_source =
      virtual_card_enrollment_source;
}

bool VirtualCardEnrollmentManager::
    IsValidGetDetailsForEnrollmentResponseDetails(
        const payments::PaymentsNetworkInterface::
            GetDetailsForEnrollmentResponseDetails&
                get_details_for_enrollment_response_details) {
  if (get_details_for_enrollment_response_details.google_legal_message.empty())
    return false;

  if (get_details_for_enrollment_response_details.vcn_context_token.empty())
    return false;

  return true;
}

}  // namespace autofill
