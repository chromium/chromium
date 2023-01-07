// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_FLOW_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_FLOW_H_

namespace autofill {

// This enum is used to denote the specific source that the virtual card
// enrollment process originated from.
enum class VirtualCardEnrollmentSource {
  // Default value, should never be used.
  kNone = 0,
  // Offering VCN Enrollment after Upstream, i.e., saving a card to Google
  // Payments.
  kUpstream = 1,
  // Offering VCN Enrollment after Downstream, i.e., unmasking a card from
  // Google Payments.
  kDownstream = 2,
  // Offering VCN Enrollment from the payment methods settings page.
  kSettingsPage = 3,
  // Max value, needs to be updated every time a new enum is added.
  kMaxValue = kSettingsPage,
};

// Denotes the request type for an UpdateVirtualCardEnrollmentRequest.
enum class VirtualCardEnrollmentRequestType {
  // Default value, should never be used.
  kNone = 0,
  // The corresponding UpdateVirtualCardEnrollmentRequest is an enroll
  // request.
  kEnroll = 1,
  // The corresponding UpdateVirtualCardEnrollmentRequest is an unenroll
  // request.
  kUnenroll = 2,
  // Max value, needs to be updated every time a new enum is added.
  kMaxValue = kUnenroll,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_FLOW_H_
