// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTEXT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTEXT_H_

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
  // kMaxValue is required for logging histograms.
  kMaxValue = kTypeUnknown,
};

// The context for the autofill error dialog.
struct AutofillErrorDialogContext {
  // The type of autofill error dialog that will be displayed.
  AutofillErrorDialogType type = AutofillErrorDialogType::kTypeUnknown;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_ERROR_DIALOG_CONTEXT_H_
