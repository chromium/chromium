// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_SAVE_METRICS_IOS_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_SAVE_METRICS_IOS_H_

#import <stddef.h>

#import <string>
#import <string_view>

#import "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::autofill_metrics {

// Metrics to track events when a credit card dialog (banner, modal) is offered
// on iOS. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
// LINT.IfChange(SaveCreditCardPromptResultIOS)
enum class SaveCreditCardPromptResultIOS {
  // Dialog shown to user.
  kShown = 0,
  // Dialog accepted.
  kAccepted = 1,
  // User explicitly denied the modal by tapping the `Close` button.
  kDenied = 2,
  // Banner swiped away.
  kSwiped = 3,
  // Banner timed out.
  KTimedOut = 4,
  // User clicked a link on the modal, which caused the dialog to close.
  kLinkClicked = 5,
  // User did not interact with the modal.
  kIgnored = 6,
  kMaxValue = kIgnored,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:SaveCreditCardPromptResultIOS)

// Enum describing the types of overlays displayed for saving a card on iOS.
enum class SaveCreditCardPromptOverlayType {
  // TODO(crbug.com/391366699): Add `kBottomSheet` as an overlay type
  kBanner,
  kModal,
};

// Logs events that happen when the prompt to save a credit card (locally or to
// the server) is shown and user's subsequent action for that prompt. The logged
// histogram also records the number of strikes (i.e number of times the card
// has previously been rejected to be saved) and whether a fix flow was required
// due to missing information (e.g., cardholder name or expiration date).
void LogSaveCreditCardPromptResultIOS(
    SaveCreditCardPromptResultIOS metric,
    bool is_uploading,
    const payments::PaymentsAutofillClient::SaveCreditCardOptions& options,
    SaveCreditCardPromptOverlayType overlay_type);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_CREDIT_CARD_SAVE_METRICS_IOS_H_
