// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTEXT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTEXT_H_

#include <optional>
#include <string>

namespace autofill {

// Keep in sync with `AutofillErrorDialogType` in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AutofillErrorDialogType {
  // Error shown when the server returns a temporary error for unmasking a
  // virtual card.
  kVirtualCardTemporaryError = 0,
  // Error shown when the server returns a permanent error for unmasking a
  // virtual card.
  kVirtualCardPermanentError = 1,
  // Error shown when the server says that the virtual card being unmasked is
  // not eligible for the virtual card feature.
  kVirtualCardNotEligibleError = 2,
  // Default value, should never be used.
  kTypeUnknown = 3,
  // Error shown when the server returns a permanent error for unmasking a
  // masked server card.
  kMaskedServerCardRiskBasedUnmaskingPermanentError = 4,
  // Error shown when the card cannot be verified because Google Payments
  // servers can't be reached.
  kMaskedServerCardRiskBasedUnmaskingNetworkError = 5,
  // Error shown when the server returns a temporary error for unmasking a
  // server IBAN.
  kMaskedServerIbanUnmaskingTemporaryError = 6,
  // Error shown when the server returns result for credit card upload as
  // unsuccessful.
  kCreditCardUploadError = 7,
  // Error shown when the server cannot enroll the virtual card.
  kVirtualCardEnrollmentTemporaryError = 8,
  // kMaxValue is required for logging histograms.
  kMaxValue = kVirtualCardEnrollmentTemporaryError,
};

// The context for the autofill error dialog.
struct AutofillErrorDialogContext {
  // Returns an AutofillErrorDialogContext that is type
  // kVirtualCardPermanentError if `is_permanent_error` is true, and type
  // kVirtualCardTemporaryError if `is_permanent_error` is false.
  static AutofillErrorDialogContext WithVirtualCardPermanentOrTemporaryError(
      bool is_permanent_error);

  AutofillErrorDialogContext();
  AutofillErrorDialogContext(const AutofillErrorDialogContext& other);
  AutofillErrorDialogContext(AutofillErrorDialogContext&& other);
  AutofillErrorDialogContext& operator=(const AutofillErrorDialogContext&);
  AutofillErrorDialogContext& operator=(AutofillErrorDialogContext&&);
  ~AutofillErrorDialogContext();

  bool operator==(const AutofillErrorDialogContext& other_context) const;

  // The type of autofill error dialog that will be displayed.
  AutofillErrorDialogType type = AutofillErrorDialogType::kTypeUnknown;

  // Autofill error dialog title returned from the server. Present in situations
  // where the server returns an error, and wants to display a detailed title
  // related to the error to the user. This should be preferred for the title of
  // the autofill error dialog if a value is present. The language is based on
  // the client's locale.
  std::optional<std::string> server_returned_title;

  // Autofill error dialog description returned from the server. Present in
  // situations where the server returns an error, and wants to display a
  // detailed description related to the error to the user. This should be
  // preferred for the description of the autofill error dialog if a value is
  // present. The language is based on the client's locale.
  std::optional<std::string> server_returned_description;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTEXT_H_
