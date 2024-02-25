// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_MANUAL_FALLBACK_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_MANUAL_FALLBACK_METRICS_H_

#include <string_view>
#include "components/autofill/core/browser/filling_product.h"

namespace autofill::autofill_metrics {

// Metrics logger when autofill is triggered from either an unclassified field
// or a field that does not match the target `FillingProduct`, for instance when
// an user uses address fallback on a field classified as credit card. Like
// other form event loggers, the lifetime of this class is attached to that of
// the BrowserAutofillManager. It collects events until it is destroyed, at
// which point metrics are emitted. Context menu related tests are in
// autofill_context_menu_manager_unittest.cc.
class ManualFallbackEventLogger {
 public:
  // Emits metrics before destruction.
  ~ManualFallbackEventLogger();

  // Called when context menu was opened on a qualifying field.
  // `address_fallback_present` indicates where the address fallback was
  // added. Similarly, `credit_cards_fallback_present` indicates whether a
  // credit_card fallback option was added.
  void ContextMenuEntryShown(bool address_fallback_present,
                             bool credit_cards_fallback_present);

  // Called when a fallback option was accepted (not just hovered).
  // `target_filling_product` specifies which of the available options was
  // chosen.
  void ContextMenuEntryAccepted(FillingProduct target_filling_product);

 private:
  enum class ContextMenuEntryState { kNotShown = 0, kShown = 1, kAccepted = 2 };

  // If according to the `state` the context menu was used, emits into the
  // `bucket` (address or credit_card) whether an entry was accepted or not
  void EmitExplicitlyTriggeredMetric(ContextMenuEntryState state,
                                     std::string_view bucket);

  // For addresses and credit cards filling, tracks if the manual fallback
  // context menu entry was shown or accepted.
  ContextMenuEntryState not_classified_as_target_filling_address =
      ContextMenuEntryState::kNotShown;
  ContextMenuEntryState not_classified_as_target_filling_credit_card =
      ContextMenuEntryState::kNotShown;
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_MANUAL_FALLBACK_METRICS_H_
