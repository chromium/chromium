// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CLIENT_BEHAVIOR_CONSTANTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CLIENT_BEHAVIOR_CONSTANTS_H_

namespace autofill {
enum class ClientBehaviorConstants {
  // ClientBehaviorConstant encompasses all the active client behaviors for the
  // browser during the outgoing calls to the Payments server.
  // Active client behaviors are the enums outlined below which tell a specific
  // feature/experiment that is triggered/active on the browser.
  // These enum flags are persisted to the Payments server logs. Entries should
  // not be renumbered and numeric values should never be reused.
  // Note that the number of the behavior, not the name, is sent in the JSON
  // request to Payments.

  // Originally used for the AutofillEnableNewSaveCardBubbleUi rollout. This
  // enum was provided to certain upload card requests between M113 and M123 in
  // order to retrieve the correct TOS footer from the Payments server. It is
  // now deprecated and should no longer be used.
  kUsingFasterAndProtectedUi = 1,

  // From M114 onwards, this enum is added to the client_behavior_signals in the
  // UnmaskCardRequest when a selected suggestion contains the credit card's
  // metadata (both card art image AND card product name to be shown).
  kShowingCardArtImageAndCardProductName = 2,

  // For more information on this signal, see
  // kAutofillEnableCvcStorageAndFilling flag. This enum is to be included in
  // the client_behavior_signals as this retrieves the correct TOS in the footer
  // for Chrome to offer to save a card's CVC.
  kOfferingToSaveCvc = 3,

  // Some UIs are expected to show the account email appended to the legal
  // message. The payments server will use the presence of this constant to
  // determine whether to send a legal message that includes the account email.
  kShowAccountEmailInLegalMessage = 4,

  // This enum reflects that the selected credit card suggestion contained a
  // label with a applicable credit card benefit.
  kShowingCardBenefits = 5,
};
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CLIENT_BEHAVIOR_CONSTANTS_H_
