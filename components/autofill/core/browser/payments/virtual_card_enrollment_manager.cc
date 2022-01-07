// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

namespace autofill {

VirtualCardEnrollmentFields::VirtualCardEnrollmentFields() = default;
VirtualCardEnrollmentFields::~VirtualCardEnrollmentFields() = default;

VirtualCardEnrollmentProcessState::VirtualCardEnrollmentProcessState() =
    default;
VirtualCardEnrollmentProcessState::~VirtualCardEnrollmentProcessState() =
    default;

VirtualCardEnrollmentManager::VirtualCardEnrollmentManager(
    raw_ptr<AutofillClient> client,
    const std::string& app_locale) {}

VirtualCardEnrollmentManager::~VirtualCardEnrollmentManager() = default;

void VirtualCardEnrollmentManager::OfferVirtualCardEnroll(
    raw_ptr<CreditCard> credit_card,
    VirtualCardEnrollmentSource virtual_card_enrollment_source) {}

void VirtualCardEnrollmentManager::Unenroll(int64_t instrument_id) {}

void OnRiskDataLoadedForVirtualCard(
    std::unique_ptr<VirtualCardEnrollmentProcessState> state,
    const std::string& risk_data) {}

void GetDetailsForEnroll(
    std::unique_ptr<VirtualCardEnrollmentProcessState> state) {}

void OnDidGetDetailsForEnrollResponse(
    std::unique_ptr<VirtualCardEnrollmentProcessState> state,
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::GetDetailsForEnrollmentResponseDetails
        get_details_for_enrollment_response_fields) {}

void VirtualCardEnrollmentManager::ShowVirtualCardEnrollmentBubble(
    std::unique_ptr<VirtualCardEnrollmentProcessState> state) {}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleAccepted(
    raw_ptr<CreditCard> credit_card) {}

void VirtualCardEnrollmentManager::OnDidGetUpdateVirtualCardEnrollmentResponse(
    AutofillClient::PaymentsRpcResult result) {}

void VirtualCardEnrollmentManager::OnVirtualCardEnrollmentBubbleCancelled() {}

}  // namespace autofill
