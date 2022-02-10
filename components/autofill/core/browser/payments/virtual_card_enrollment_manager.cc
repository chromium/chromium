// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/strike_database_base.h"
#include "ui/gfx/image/image.h"

namespace autofill {

VirtualCardEnrollmentFields::VirtualCardEnrollmentFields() = default;
VirtualCardEnrollmentFields::VirtualCardEnrollmentFields(
    const VirtualCardEnrollmentFields&) = default;
VirtualCardEnrollmentFields& VirtualCardEnrollmentFields::operator=(
    const VirtualCardEnrollmentFields&) = default;
VirtualCardEnrollmentFields::~VirtualCardEnrollmentFields() = default;

VirtualCardEnrollmentProcessState::VirtualCardEnrollmentProcessState() =
    default;
VirtualCardEnrollmentProcessState::VirtualCardEnrollmentProcessState(
    const VirtualCardEnrollmentProcessState&) = default;
VirtualCardEnrollmentProcessState& VirtualCardEnrollmentProcessState::operator=(
    const VirtualCardEnrollmentProcessState&) = default;

VirtualCardEnrollmentProcessState::~VirtualCardEnrollmentProcessState() =
    default;

VirtualCardEnrollmentManager::VirtualCardEnrollmentManager(
    raw_ptr<AutofillClient> autofill_client,
    raw_ptr<PersonalDataManager> personal_data_manager)
    : autofill_client_(autofill_client),
      payments_client_(autofill_client->GetPaymentsClient()),
      personal_data_manager_(personal_data_manager) {
  // Here only check autofill_client_ because in some tests payments_client_
  // does not exist.
  DCHECK(autofill_client_);

  if (autofill_client->GetStrikeDatabase()) {
    virtual_card_enrollment_strike_database_ =
        std::make_unique<VirtualCardEnrollmentStrikeDatabase>(
            autofill_client->GetStrikeDatabase());
  }
}

VirtualCardEnrollmentManager::~VirtualCardEnrollmentManager() = default;

void VirtualCardEnrollmentManager::OfferVirtualCardEnroll(
    raw_ptr<CreditCard> credit_card,
    VirtualCardEnrollmentSource virtual_card_enrollment_source) {
  DCHECK(credit_card);
  DCHECK_NE(virtual_card_enrollment_source, VirtualCardEnrollmentSource::kNone);
  if (IsVirtualCardEnrollmentBlocked(credit_card,
                                     virtual_card_enrollment_source)) {
    return;
  }

  state_.virtual_card_enrollment_fields.credit_card = credit_card;

  // The |card_art_image| might not be synced yet from the sync server which
  // will result in a nullptr. This situation can occur in the upstream flow.
  // If it is not synced, GetCreditCardArtImageForUrl() will send a fetch
  // request to sync the |card_art_image|, and before showing the
  // VirtualCardEnrollmentBubble we will try to fetch the |card_art_image|
  // from the local cache.
  state_.virtual_card_enrollment_fields.card_art_image =
      personal_data_manager_->GetCreditCardArtImageForUrl(
          credit_card->card_art_url());

  state_.virtual_card_enrollment_fields.virtual_card_enrollment_source =
      virtual_card_enrollment_source;
  autofill_client_->LoadRiskData(base::BindOnce(
      &VirtualCardEnrollmentManager::OnRiskDataLoadedForVirtualCard,
      weak_ptr_factory_.GetWeakPtr()));
}

void VirtualCardEnrollmentManager::Unenroll(int64_t instrument_id) {
  payments::PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails
      request_details;

  // Unenroll can only happen from the settings page.
  request_details.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kSettingsPage;

  request_details.virtual_card_enrollment_request_type =
      VirtualCardEnrollmentRequestType::kUnenroll;
  request_details.billing_customer_number =
      payments::GetBillingCustomerId(personal_data_manager_);
  request_details.instrument_id = instrument_id;

  payments_client_->UpdateVirtualCardEnrollment(
      request_details,
      base::BindOnce(&VirtualCardEnrollmentManager::
                         OnDidGetUpdateVirtualCardEnrollmentResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool VirtualCardEnrollmentManager::IsVirtualCardEnrollmentBlocked(
    raw_ptr<CreditCard> credit_card,
    VirtualCardEnrollmentSource virtual_card_enrollment_source) const {
  if (virtual_card_enrollment_source ==
      VirtualCardEnrollmentSource::kSettingsPage)
    return false;

  if (!GetVirtualCardEnrollmentStrikeDatabase())
    return false;

  return GetVirtualCardEnrollmentStrikeDatabase()->IsMaxStrikesLimitReached(
      credit_card->guid());
}

void VirtualCardEnrollmentManager::
    AddStrikeToBlockOfferingVirtualCardEnrollment(const std::string& guid) {
  if (!GetVirtualCardEnrollmentStrikeDatabase())
    return;

  GetVirtualCardEnrollmentStrikeDatabase()->AddStrike(guid);
}

void VirtualCardEnrollmentManager::
    RemoveAllStrikesToBlockOfferingVirtualCardEnrollment(
        const std::string& guid) {
  if (!GetVirtualCardEnrollmentStrikeDatabase())
    return;

  GetVirtualCardEnrollmentStrikeDatabase()->ClearStrikes(guid);
}

void VirtualCardEnrollmentManager::OnDidGetUpdateVirtualCardEnrollmentResponse(
    AutofillClient::PaymentsRpcResult result) {
  Reset();
}

void VirtualCardEnrollmentManager::Reset() {
  payments_client_->CancelRequest();
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_ = VirtualCardEnrollmentProcessState();
}

VirtualCardEnrollmentStrikeDatabase*
VirtualCardEnrollmentManager::GetVirtualCardEnrollmentStrikeDatabase() const {
  return virtual_card_enrollment_strike_database_.get();
}

void VirtualCardEnrollmentManager::OnRiskDataLoadedForVirtualCard(
    const std::string& risk_data) {
  state_.risk_data = risk_data;
  GetDetailsForEnroll();
}

void VirtualCardEnrollmentManager::GetDetailsForEnroll() {
  payments::PaymentsClient::GetDetailsForEnrollmentRequestDetails
      request_details;
  request_details.app_locale = personal_data_manager_->app_locale();
  request_details.risk_data = state_.risk_data.value_or("");
  request_details.billing_customer_number =
      payments::GetBillingCustomerId(personal_data_manager_);
  request_details.instrument_id =
      state_.virtual_card_enrollment_fields.credit_card->instrument_id();
  request_details.source =
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source;

  payments_client_->GetVirtualCardEnrollmentDetails(
      request_details,
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnDidGetDetailsForEnrollResponse,
          weak_ptr_factory_.GetWeakPtr()));
}

void VirtualCardEnrollmentManager::OnDidGetDetailsForEnrollResponse(
    AutofillClient::PaymentsRpcResult result,
    const payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails&
        response) {
  // Show the virtual card permanent error dialog if server explicitly returned
  // permanent error, show temporary error dialog for the rest of the failure
  // cases since currently only virtual card is supported.
  if (result != AutofillClient::PaymentsRpcResult::kSuccess) {
    autofill_client_->ShowVirtualCardErrorDialog(
        /*is_permanent_error=*/result ==
        AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure);
    Reset();
    return;
  }

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

  // Tries to get the card art image again from the local cache. If the card art
  // image is not available, then |state_|'s |virtual_card_enrollment_fields|'s
  // |card_art_image| will be nullptr. The view will set it to the network image
  // if it ends up being nullptr. The card art image might not be present
  // in the upstream flow if the sync server has not synced the card art image
  // yet.
  if (!state_.virtual_card_enrollment_fields.card_art_image) {
    state_.virtual_card_enrollment_fields.card_art_image =
        personal_data_manager_->GetCachedCardArtImageForUrl(
            state_.virtual_card_enrollment_fields.credit_card->card_art_url());
  }

  ShowVirtualCardEnrollmentBubble();
}

void VirtualCardEnrollmentManager::ShowVirtualCardEnrollmentBubble() {
  autofill_client_->ShowVirtualCardEnrollDialog(
      &(state_.virtual_card_enrollment_fields),
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleAccepted,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled,
          weak_ptr_factory_.GetWeakPtr()));
}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleAccepted() {
  payments::PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails
      request_details;
  request_details.virtual_card_enrollment_source =
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source;
  request_details.virtual_card_enrollment_request_type =
      VirtualCardEnrollmentRequestType::kEnroll;
  request_details.billing_customer_number =
      payments::GetBillingCustomerId(personal_data_manager_);
  request_details.vcn_context_token = state_.vcn_context_token;

  payments_client_->UpdateVirtualCardEnrollment(
      request_details,
      base::BindOnce(&VirtualCardEnrollmentManager::
                         OnDidGetUpdateVirtualCardEnrollmentResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  RemoveAllStrikesToBlockOfferingVirtualCardEnrollment(
      state_.virtual_card_enrollment_fields.credit_card->guid());
}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled() {
  AddStrikeToBlockOfferingVirtualCardEnrollment(
      state_.virtual_card_enrollment_fields.credit_card->guid());
  Reset();
}

}  // namespace autofill
