// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UI_UTILS_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UI_UTILS_H_

namespace payments::facilitated {

// This enum is used to denote the UI screen shown by the feature in the
// Facilitated Payments bottom sheet.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(UiState)
enum class UiState {
  // Represents no UI being shown.
  kHidden = 0,
  // Represents FOP selector being shown.
  kFopSelector = 1,
  // Represents progress screen being shown.
  kProgressScreen = 2,
  // Represents error screen being shown.
  kErrorScreen = 3,
  // Max value, needs to be updated every time a new enum is added.
  kMaxValue = kErrorScreen,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/facilitated_payments/enums.xml:FacilitatedPayments.UiScreen)

// This enum is used to denote the UI events in the Facilitated Payments bottom
// sheet.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.facilitated_payments.core.ui_utils)
enum class UiEvent {
  // Represents a new screen being shown. This includes both opening the bottom
  // sheet to show a screen, and replacing an existing screen to show a new
  // screen.
  kNewScreenShown = 0,
  // Represents that a new screen was requested to be shown, but the bottom
  // sheet failed to open.
  kScreenCouldNotBeShown = 1,
  // Represents the bottom sheet being closed where the user did not close the
  // bottom sheet.
  kScreenClosedNotByUser = 2,
  // Represents the bottom sheet being closed by the user.
  kScreenClosedByUser = 3,
  // Max value, needs to be updated every time a new enum is added.
  kMaxValue = kScreenClosedByUser,
};

// This enum is used to denote user actions on the FOP selector.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FopSelectorAction)
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.facilitated_payments.core.ui_utils)
enum class FopSelectorAction {
  // User selected a FOP for payment.
  kFopSelected = 0,
  // User clicked on the link to disable payment for the type of FOP shown in
  // the FOP selector. The link takes them to the settings page where the type
  // of FOPs shown can be managed.
  kTurnOffPaymentPromptLinkClicked = 1,
  // User selected the "Manage payment accounts" option. This takes them to
  // Chrome's Payments settings page.
  kManagePaymentMethodsOptionSelected = 2,
  kMaxValue = kManagePaymentMethodsOptionSelected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/facilitated_payments/enums.xml:FacilitatedPayments.FopSelectorAction)

// This enum is used to denote user actions on the non-card FOP selector.
//
// LINT.IfChange(PaymentLinkFopSelectorAction)
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.facilitated_payments.core.ui_utils)
enum class PaymentLinkFopSelectorAction {
  // User has selected eWallet for payment.
  kEwalletSelected = 0,
  // User has selected payment app for payment.
  kPaymentAppSelected = 1,
  // User clicked on the link to disable payment for the type of FOP shown in
  // the FOP selector. The link takes them to the settings page where the type
  // of FOPs shown can be managed.
  kTurnOffPaymentPromptLinkClicked = 2,
  // User selected the "Manage payment accounts" option. This takes them to
  // Chrome's Payments settings page.
  kManagePaymentMethodsOptionSelected = 3,
  kMaxValue = kManagePaymentMethodsOptionSelected,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/facilitated_payments/enums.xml:FacilitatedPayments.PaymentLinkFopSelectorAction)

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_UTILS_FACILITATED_PAYMENTS_UI_UTILS_H_
