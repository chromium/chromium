// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/strike_database_base.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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
    raw_ptr<PersonalDataManager> personal_data_manager,
    raw_ptr<payments::PaymentsClient> payments_client,
    raw_ptr<AutofillClient> autofill_client)
    : autofill_client_(autofill_client),
      personal_data_manager_(personal_data_manager),
      payments_client_(payments_client) {
  if (autofill_client_) {
    StrikeDatabaseBase* strike_database = autofill_client->GetStrikeDatabase();
    virtual_card_enrollment_strike_database_ =
        std::make_unique<VirtualCardEnrollmentStrikeDatabase>(strike_database);
  }
}

VirtualCardEnrollmentManager::~VirtualCardEnrollmentManager() = default;

void VirtualCardEnrollmentManager::OfferVirtualCardEnroll(
    const CreditCard& credit_card,
    VirtualCardEnrollmentSource virtual_card_enrollment_source,
    const raw_ptr<PrefService> user_prefs,
    RiskAssessmentFunction risk_assessment_function,
    VirtualCardEnrollmentFieldsLoadedCallback
        virtual_card_enrollment_fields_loaded_callback) {
  Reset();
  DCHECK_NE(virtual_card_enrollment_source, VirtualCardEnrollmentSource::kNone);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Hide the bubble and icon if it is already showing for a previous enrollment
  // bubble.
  DCHECK(autofill_client_);
  autofill_client_->HideVirtualCardEnrollBubbleAndIconIfVisible();
#endif
  state_.virtual_card_enrollment_fields.credit_card = credit_card;
  risk_assessment_function_ = std::move(risk_assessment_function);
  virtual_card_enrollment_fields_loaded_callback_ =
      std::move(virtual_card_enrollment_fields_loaded_callback);
  // The |card_art_image| might not be synced yet from the sync server which
  // will result in a nullptr. This situation can occur in the upstream flow. If
  // it is not synced, GetCreditCardArtImageForUrl() will send a fetch request
  // to sync the |card_art_image|, and before showing the
  // VirtualCardEnrollmentBubble we will try to fetch the |card_art_image| from
  // the local cache.
  state_.virtual_card_enrollment_fields.card_art_image =
      personal_data_manager_->GetCreditCardArtImageForUrl(
          credit_card.card_art_url());

  state_.virtual_card_enrollment_fields.virtual_card_enrollment_source =
      virtual_card_enrollment_source;

  LoadRiskDataAndContinueFlow(
      user_prefs,
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnRiskDataLoadedForVirtualCard,
          weak_ptr_factory_.GetWeakPtr()));
}

void VirtualCardEnrollmentManager::OnCardSavedAnimationComplete() {
  if (state_.virtual_card_enrollment_fields.virtual_card_enrollment_source ==
      VirtualCardEnrollmentSource::kUpstream) {
    avatar_animation_complete_ = true;

    if (enroll_response_details_received_)
      ShowVirtualCardEnrollBubble();
  }
}

void VirtualCardEnrollmentManager::Enroll() {
  LogUpdateVirtualCardEnrollmentRequestAttempt(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      VirtualCardEnrollmentRequestType::kEnroll);
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
                     weak_ptr_factory_.GetWeakPtr(),
                     VirtualCardEnrollmentRequestType::kEnroll));
}

void VirtualCardEnrollmentManager::Unenroll(int64_t instrument_id) {
  LogUpdateVirtualCardEnrollmentRequestAttempt(
      VirtualCardEnrollmentSource::kSettingsPage,
      VirtualCardEnrollmentRequestType::kUnenroll);

  payments::PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails
      request_details;
  state_.virtual_card_enrollment_fields.virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kSettingsPage;

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
                     weak_ptr_factory_.GetWeakPtr(),
                     VirtualCardEnrollmentRequestType::kUnenroll));
}

bool VirtualCardEnrollmentManager::IsVirtualCardEnrollmentBlocked(
    const std::string& guid) const {
  return GetVirtualCardEnrollmentStrikeDatabase() &&
         GetVirtualCardEnrollmentStrikeDatabase()->IsMaxStrikesLimitReached(
             guid);
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

void VirtualCardEnrollmentManager::SetSaveCardBubbleAcceptedTimestamp(
    const base::Time& save_card_bubble_accepted_timestamp) {
  save_card_bubble_accepted_timestamp_ =
      std::move(save_card_bubble_accepted_timestamp);
}

void VirtualCardEnrollmentManager::OnDidGetUpdateVirtualCardEnrollmentResponse(
    VirtualCardEnrollmentRequestType type,
    AutofillClient::PaymentsRpcResult result) {
  LogUpdateVirtualCardEnrollmentRequestResult(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      type, result == AutofillClient::PaymentsRpcResult::kSuccess);
  Reset();
}

void VirtualCardEnrollmentManager::Reset() {
  payments_client_->CancelRequest();
  weak_ptr_factory_.InvalidateWeakPtrs();
  state_ = VirtualCardEnrollmentProcessState();
  avatar_animation_complete_ = false;
  enroll_response_details_received_ = false;
}

VirtualCardEnrollmentStrikeDatabase*
VirtualCardEnrollmentManager::GetVirtualCardEnrollmentStrikeDatabase() const {
  return virtual_card_enrollment_strike_database_.get();
}

void VirtualCardEnrollmentManager::LoadRiskDataAndContinueFlow(
    raw_ptr<PrefService> user_prefs,
    base::OnceCallback<void(const std::string&)> callback) {
  if (autofill_client_) {
    autofill_client_->LoadRiskData(std::move(callback));
  } else {
    // No |autofill_client_| present indicates we are in the clank settings page
    // use case, so we load risk data using a method that does not require web
    // contents to be present.
    std::move(risk_assessment_function_)
        .Run(/*obfuscated_gaia_id=*/0, user_prefs, std::move(callback), nullptr,
             gfx::Rect());
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
  autofill_client_->ShowVirtualCardEnrollDialog(
      state_.virtual_card_enrollment_fields,
      base::BindOnce(&VirtualCardEnrollmentManager::Enroll,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled,
          weak_ptr_factory_.GetWeakPtr()));
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
      state_.virtual_card_enrollment_fields.credit_card.instrument_id();
  request_details.source =
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source;
  payments_client_->GetVirtualCardEnrollmentDetails(
      request_details,
      base::BindOnce(
          &VirtualCardEnrollmentManager::OnDidGetDetailsForEnrollResponse,
          weak_ptr_factory_.GetWeakPtr()));

  LogGetDetailsForEnrollmentRequestAttempt(request_details.source);
}

void VirtualCardEnrollmentManager::OnDidGetDetailsForEnrollResponse(
    AutofillClient::PaymentsRpcResult result,
    const payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails&
        response) {
  enroll_response_details_received_ = true;

  LogGetDetailsForEnrollmentRequestResult(
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source,
      /*succeeded=*/result == AutofillClient::PaymentsRpcResult::kSuccess);

  // Show the virtual card permanent error dialog if server explicitly returned
  // permanent error, show temporary error dialog for the rest of the failure
  // cases since currently only virtual card is supported.
  if (result != AutofillClient::PaymentsRpcResult::kSuccess) {
    // Showing an error dialog here would provide a confusing user experience as
    // it is an error for a flow that is not user-initiated, so we fail
    // silently.
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
            state_.virtual_card_enrollment_fields.credit_card.card_art_url());
  }

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableToolbarStatusChip) &&
      base::FeatureList::IsEnabled(
          features::kAutofillCreditCardUploadFeedback) &&
      state_.virtual_card_enrollment_fields.virtual_card_enrollment_source ==
          VirtualCardEnrollmentSource::kUpstream &&
      !avatar_animation_complete_) {
    return;
  }
#endif

  if (autofill_client_) {
    ShowVirtualCardEnrollBubble();
  } else {
    // If the `autofill_client_` is not present, it means that the request is
    // from Android settings page, thus run the callback with the
    // `virtual_card_enrollment_fields_`, which would show the enrollment
    // dialog.
    std::move(virtual_card_enrollment_fields_loaded_callback_)
        .Run(&state_.virtual_card_enrollment_fields);
  }
}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled() {
  Reset();
}

}  // namespace autofill
