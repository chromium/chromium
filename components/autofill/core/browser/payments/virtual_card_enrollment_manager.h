// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill {

enum class VirtualCardEnrollmentFlow {
  // Default value, should never be used.
  kNone = 0,
  // Offering VCN Enrollment after Upstream flow, i.e., saving a card to
  // Google Payments.
  kUpstream = 1,
  // Offering VCN Enrollment after Downstream flow, i.e., unmasking a card
  // from Google Payments.
  kDownstream = 2,
  // Offering VCN Enrollment from the payment methods settings page.
  kSettingsPage = 3,
  // Max value, needs to be updated every time a new enum is added.
  kMaxValue = kSettingsPage,
};

// This struct is passed into the controller when we show the
// VirtualCardEnrollmentBubble, and it lets the controller customize the
// bubble based on the fields in this struct. For example, we will show
// different last 4 digits of a credit card based on the |credit_card_| object
// in this struct.
struct VirtualCardEnrollmentFields {
  VirtualCardEnrollmentFields();
  VirtualCardEnrollmentFields(const VirtualCardEnrollmentFields&) = delete;
  VirtualCardEnrollmentFields& operator=(const VirtualCardEnrollmentFields&) =
      delete;
  ~VirtualCardEnrollmentFields();
  // Pointer to the credit card to enroll. The |credit_card_| object is owned
  // by PersonalDataManager.
  raw_ptr<CreditCard> credit_card_ = nullptr;
  // Raw pointer to the image for the card art. The |card_art_image_| object is
  // owned by PersonalDataManager.
  raw_ptr<gfx::Image> card_art_image_ = nullptr;
  // The legal message lines for the footer of the
  // VirtualCardEnrollmentBubble.
  LegalMessageLines legal_message_lines_;
  // The flow for which the VirtualCardEnrollmentBubble will be shown.
  VirtualCardEnrollmentFlow virtual_card_enroll_flow_ =
      VirtualCardEnrollmentFlow::kNone;
};

struct VirtualCardEnrollmentProcessState {
  VirtualCardEnrollmentProcessState();
  VirtualCardEnrollmentProcessState(const VirtualCardEnrollmentProcessState&) =
      delete;
  VirtualCardEnrollmentProcessState& operator=(
      const VirtualCardEnrollmentProcessState&) = delete;
  ~VirtualCardEnrollmentProcessState();
  // Only populated once the risk engine responded.
  absl::optional<std::string> risk_data_;
  // |credit_card_| and |virtual_card_enroll_flow_| are populated in the
  // beginning of the virtual card enrollment flow, but the rest of the fields
  // are only populated before showing the VirtualCardEnrollmentBubble.
  VirtualCardEnrollmentFields virtual_card_enrollment_fields_;
};

// Owned by FormDataImporter. There is one instance of this class per tab. This
// class manages the flow for enrolling and unenrolling in Virtual Card
// Numbers.
class VirtualCardEnrollmentManager {
 public:
  // The parameters should outlive the VirtualCardEnrollmentManager.
  VirtualCardEnrollmentManager(raw_ptr<AutofillClient> client,
                               const std::string& app_locale);
  VirtualCardEnrollmentManager(const VirtualCardEnrollmentManager&) = delete;
  VirtualCardEnrollmentManager& operator=(const VirtualCardEnrollmentManager&) =
      delete;
  ~VirtualCardEnrollmentManager();

  // Starting point for the VCN enroll flow. The fields in |credit_card| will
  // be used throughout the flow, such as for request fields as well as credit
  // card specific fields for the bubble to display.
  // |virtual_card_enrollment_flow| will be used by
  // ShowVirtualCardEnrollBubble() to differentiate different bubbles based on
  // the flow we are in.
  void OfferVirtualCardEnroll(
      raw_ptr<CreditCard> credit_card,
      VirtualCardEnrollmentFlow virtual_card_enrollment_flow);

  // Unenrolls the card mapped to the given |instrument_id|.
  void Unenroll(int64_t instrument_id);

 private:
  // Called once the risk data is loaded. The |risk_data| will be used with
  // |credit_card|'s |instrument_id_| field to make a GetDetailsForEnroll
  // request, and |virtual_card_enroll_flow| will be passed down to when we show
  // the bubble so that we show the correct bubble version.
  void OnRiskDataLoadedForVirtualCard(
      std::unique_ptr<VirtualCardEnrollmentProcessState> state,
      const std::string& risk_data);

  // Sends the GetDetailsForEnrollRequest using AutofillClient's
  // |payments_client_|. |state|'s |risk_data| and |credit_card|'s
  // |instrument_id| are the fields the server requires for the
  // GetDetailsForEnrollRequest, and will be used by |payments_client_|.
  // |state|'s |virtual_card_enrollment_fields_|'s
  // |virtual_card_enrollment_flow| is passed here so that it can be forwarded
  // to ShowVirtualCardEnrollBubble.
  void GetDetailsForEnroll(
      std::unique_ptr<VirtualCardEnrollmentProcessState> state);

  // Handles the response from the GetDetailsForEnrollRequest.
  void OnDidGetDetailsForEnrollResponse(
      std::unique_ptr<VirtualCardEnrollmentProcessState> state,
      AutofillClient::PaymentsRpcResult result,
      const payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails&
          get_details_for_enrollment_response_fields);

  // Shows the VirtualCardEnrollmentBubble. |state|'s
  // |virtual_card_enrollment_fields| will contain all of the dynamic fields
  // VirtualCardEnrollmentBubbleController needs to display the correct bubble.
  void ShowVirtualCardEnrollmentBubble(
      std::unique_ptr<VirtualCardEnrollmentProcessState> state);

  // Uses AutofillClient's |payments_client_| to send the enroll request when
  // the user accepts the bubble. |vcn_context_token_|, which
  // should be set when we receive the GetDetailsForEnrollResponse, is used in
  // the enroll request to enroll the correct card.
  void OnVirtualCardEnrollmentBubbleAccepted(raw_ptr<CreditCard> credit_card);

  // Handles the response from the Update Virtual Card Enrollment Request.
  void OnDidGetUpdateVirtualCardEnrollmentResponse(
      CreditCard::VirtualCardEnrollmentState virtual_card_enrollment_state);

  // Cancels the entire Virtual Card Enrollment flow.
  void OnVirtualCardEnrollmentBubbleCancelled();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
