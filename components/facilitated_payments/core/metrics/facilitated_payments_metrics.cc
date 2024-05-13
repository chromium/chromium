// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace payments::facilitated {

void LogIsApiAvailableResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.IsApiAvailable.Result",
                        result);
  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.IsApiAvailable.Latency",
                              duration);
}

void LogGetClientTokenResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.GetClientToken.Result",
                        result);
  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.GetClientToken.Latency",
                              duration);
}

void LogPaymentNotOfferedReason(PaymentNotOfferedReason reason) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  base::UmaHistogramEnumeration(
      "FacilitatedPayments.Pix.PaymentNotOfferedReason", reason);
}

void LogInitiatePaymentResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.InitiatePayment.Result",
                        result);
  base::UmaHistogramLongTimes("FacilitatedPayments.Pix.InitiatePayment.Latency",
                              duration);
}

void LogInitiatePurchaseActionResult(bool result, base::TimeDelta duration) {
  // TODO(b/337929926): Remove hardcoding for Pix and use
  // FacilitatedPaymentsType enum.
  UMA_HISTOGRAM_BOOLEAN("FacilitatedPayments.Pix.InitiatePurchaseAction.Result",
                        result);
  base::UmaHistogramLongTimes(
      "FacilitatedPayments.Pix.InitiatePurchaseAction.Latency", duration);
}

}  // namespace payments::facilitated
