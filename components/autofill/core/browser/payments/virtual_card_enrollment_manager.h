// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {

class CreditCard;

// This struct is passed into the controller when we show the
// VirtualCardEnrollmentBubble, and it lets the controller customize the
// bubble based on the fields in this struct. For example, we will show
// different last 4 digits of a credit card based on the |credit_card| object
// in this struct.
struct VirtualCardEnrollmentFields {
  VirtualCardEnrollmentFields();
  VirtualCardEnrollmentFields(const VirtualCardEnrollmentFields&) = delete;
  VirtualCardEnrollmentFields& operator=(const VirtualCardEnrollmentFields&) =
      delete;
  ~VirtualCardEnrollmentFields();
  // Pointer to the credit card to enroll. The |credit_card| object is owned
  // by PersonalDataManager.
  raw_ptr<CreditCard> credit_card = nullptr;
  // Raw pointer to the image for the card art. The |card_art_image| object is
  // owned by PersonalDataManager.
  raw_ptr<gfx::Image> card_art_image = nullptr;
  // The legal message lines for the footer of the
  // VirtualCardEnrollmentBubble.
  LegalMessageLines legal_message_lines;
  // The source for which the VirtualCardEnrollmentBubble will be shown.
  VirtualCardEnrollmentSource virtual_card_enrollment_source =
      VirtualCardEnrollmentSource::kNone;
};

// This struct is used to track the state of the virtual card enrollment
// process, and its members are read from and written to throughout the process
// where needed. It is created and owned by VirtualCardEnrollmentManager.
struct VirtualCardEnrollmentProcessState {
  VirtualCardEnrollmentProcessState();
  VirtualCardEnrollmentProcessState(const VirtualCardEnrollmentProcessState&) =
      delete;
  VirtualCardEnrollmentProcessState& operator=(
      const VirtualCardEnrollmentProcessState&) = delete;
  ~VirtualCardEnrollmentProcessState();
  // Only populated once the risk engine responded.
  absl::optional<std::string> risk_data;
  // |virtual_card_enrollment_fields|'s |credit_card| and
  // |virtual_card_enrollment_source| are populated in the beginning of the
  // virtual card enrollment flow, but the rest of the fields are only populated
  // before showing the VirtualCardEnrollmentBubble.
  VirtualCardEnrollmentFields virtual_card_enrollment_fields;
  // Populated after the GetDetailsForEnrollResponseDetails are received. Based
  // on the |vcn_context_token| the server is able to retrieve the instrument
  // id, and using |vcn_context_token| for enroll allows the server to link a
  // GetDetailsForEnrollRequest with the corresponding
  // UpdateVirtualCardEnrollmentRequest for the enroll process.
  absl::optional<std::string> vcn_context_token;
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
  // |virtual_card_enrollment_source| will be used by
  // ShowVirtualCardEnrollBubble() to differentiate different bubbles based on
  // the source we originated from.
  void OfferVirtualCardEnroll(
      raw_ptr<CreditCard> credit_card,
      VirtualCardEnrollmentSource virtual_card_enrollment_source);

  // Unenrolls the card mapped to the given |instrument_id|.
  void Unenroll(int64_t instrument_id);

 private:
  // Called once the risk data is loaded. The |risk_data| will be used with
  // |state|'s |virtual_card_enrollment_fields|'s |credit_card|'s
  // |instrument_id_| field to make a GetDetailsForEnroll request, and
  // |state|'s |virtual_card_enrollment_source| will be passed down to when we
  // show the bubble so that we show the correct bubble version.
  void OnRiskDataLoadedForVirtualCard(
      std::unique_ptr<VirtualCardEnrollmentProcessState> state,
      const std::string& risk_data);

  // TODO(crbug.com/1281695): Add |client_| data member.
  // Sends the GetDetailsForEnrollRequest using |client_|'s
  // |payments_client_|. |state|'s |risk_data| and its
  // |virtual_card_enrollment_fields|'s |credit_card|'s |instrument_id| are the
  // fields the server requires for the GetDetailsForEnrollRequest, and will be
  // used by |client_|'s |payments_client_|. |state|'s
  // |virtual_card_enrollment_fields_|'s |virtual_card_enrollment_source| is
  // passed here so that it can be forwarded to ShowVirtualCardEnrollBubble.
  void GetDetailsForEnroll(
      std::unique_ptr<VirtualCardEnrollmentProcessState> state);

  // Handles the response from the GetDetailsForEnrollRequest. |result| and
  // |get_details_for_enrollment_response_fields| are received from the
  // GetDetailsForEnroll server call response, while |state| is passed down from
  // GetDetailsForEnroll() to track the current process' state.
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

  // TODO(crbug.com/1281695): Add |client_| data member.
  // Uses |client_|'s |payments_client_| to send the enroll request when
  // the user accepts the bubble. |state|'s |vcn_context_token_|, which
  // should be set when we receive the GetDetailsForEnrollResponse, is used in
  // the UpdateVirtualCardEnrollmentRequest to enroll the correct card.
  void OnVirtualCardEnrollmentBubbleAccepted(raw_ptr<CreditCard> credit_card);

  // Handles the response from the UpdateVirtualCardEnrollmentRequest.
  // |result| represents the result from the server call to change the virtual
  // card enrollment state for the credit card passed into
  // OfferVirtualCardEnroll().
  void OnDidGetUpdateVirtualCardEnrollmentResponse(
      AutofillClient::PaymentsRpcResult result);

  // Cancels the entire Virtual Card Enrollment process.
  void OnVirtualCardEnrollmentBubbleCancelled();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_MANAGER_H_
