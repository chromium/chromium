// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_METRICS_H_

namespace autofill::autofill_metrics {

// Metrics to measure user interaction with the virtual card manual fallback
// bubble after it has appeared upon unmasking and filling a virtual card.
enum class VirtualCardManualFallbackBubbleResult {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  // The reason why the bubble is closed is not clear. Possible reason is the
  // logging function is invoked before the closed reason is correctly set.
  kUnknown = 0,
  // The user explicitly closed the bubble with the close button or ESC.
  kClosed = 1,
  // The user did not interact with the bubble.
  kNotInteracted = 2,
  // Deprecated: kLostFocus = 3,
  kMaxValue = kNotInteracted,
};

// Metric to measure which field in the virtual card manual fallback bubble
// was selected by the user.
enum class VirtualCardManualFallbackBubbleFieldClicked {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.

  kCardNumber = 0,
  kExpirationMonth = 1,
  kExpirationYear = 2,
  kCardholderName = 3,
  kCVC = 4,
  kMaxValue = kCVC,
};

void LogVirtualCardManualFallbackBubbleShown(bool is_reshow);

void LogVirtualCardManualFallbackBubbleResultMetric(
    VirtualCardManualFallbackBubbleResult metric,
    bool is_reshow);

void LogVirtualCardManualFallbackBubbleFieldClicked(
    VirtualCardManualFallbackBubbleFieldClicked metric);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_METRICS_H_
