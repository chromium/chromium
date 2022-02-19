// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_

#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"

namespace autofill {

// GetDetailsForEnrollmentRequest related metrics. Attempts and results should
// be 1:1 mapping.
void LogGetDetailsForEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source);
void LogGetDetailsForEnrollmentRequestResult(VirtualCardEnrollmentSource source,
                                             bool succeeded);

// UpdateVirtualCardEnrollmentRequest related metrics. Attempts and results
// should be 1:1 mapping.
void LogUpdateVirtualCardEnrollmentRequestAttempt(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type);
void LogUpdateVirtualCardEnrollmentRequestResult(
    VirtualCardEnrollmentSource source,
    VirtualCardEnrollmentRequestType type,
    bool succeeded);
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_ENROLLMENT_METRICS_H_
